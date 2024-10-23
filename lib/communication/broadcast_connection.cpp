
#include "communication/broadcast_connection.h"

RetransmissionEntry::RetransmissionEntry() {}

BroadcastConnection::BroadcastConnection(
    NodeMap& nodes,
    const Node& local_node,
    std::map<std::string, Connection>& connections,
    BufferSet<std::string>& connection_update_buffer,
    Buffer<Message>& deliver_buffer,
    Pipeline& pipeline
) :
    nodes(nodes),
    local_node(local_node),
    connections(connections),
    pipeline(pipeline),
    ab_dispatcher(BROADCAST_ID, connection_update_buffer, pipeline),
    dispatcher(BROADCAST_ID, connection_update_buffer, pipeline),
    connection_update_buffer(connection_update_buffer),
    deliver_buffer(deliver_buffer),
    ab_sequence_number({0, 0})
{
    observe_pipeline();
}

const TransmissionDispatcher& BroadcastConnection::get_dispatcher() const { return dispatcher; }

const TransmissionDispatcher& BroadcastConnection::get_ab_dispatcher() const { return ab_dispatcher; }

void BroadcastConnection::observe_pipeline() {
    obs_receive_synchronization.on(std::bind(&BroadcastConnection::receive_synchronization, this, _1));
    obs_connection_established.on(std::bind(&BroadcastConnection::connection_established, this, _1));
    obs_connection_closed.on(std::bind(&BroadcastConnection::connection_closed, this, _1));
    obs_packet_received.on(std::bind(&BroadcastConnection::packet_received, this, _1));
    obs_message_received.on(std::bind(&BroadcastConnection::message_received, this, _1));
    obs_transmission_fail.on(std::bind(&BroadcastConnection::transmission_fail, this, _1));
    obs_transmission_complete.on(std::bind(&BroadcastConnection::transmission_complete, this, _1));
    pipeline.attach(obs_receive_synchronization);
    pipeline.attach(obs_connection_established);
    pipeline.attach(obs_connection_closed);
    pipeline.attach(obs_packet_received);
    pipeline.attach(obs_message_received);
    pipeline.attach(obs_transmission_complete);
    pipeline.attach(obs_transmission_fail);
}

void BroadcastConnection::receive_synchronization(const ReceiveSynchronization& event) {
    const std::string& id = event.node.get_id();

    if (!sequence_numbers.contains(id)) sequence_numbers.emplace(id, SequenceNumber());
    SequenceNumber& number = sequence_numbers.at(id);
    number.initial_number = event.expected_broadcast_number;
    number.next_number = event.expected_broadcast_number;
    log_info("Broadcast sequence with node ", id, " was resync to ", number.initial_number);

    if (ab_sequence_number.next_number >= event.expected_ab_number) return;
    ab_sequence_number.initial_number = event.expected_ab_number;
    ab_sequence_number.next_number = event.expected_ab_number;
    ab_dispatcher.reset_number(ab_sequence_number.initial_number);
    log_info("Atomic broadcast number synchronized to ", ab_sequence_number.initial_number, ".");
}
void BroadcastConnection::connection_established(const ConnectionEstablished&) {
    request_update();
}

void BroadcastConnection::connection_closed(const ConnectionClosed& event) {
    ab_dispatcher.cancel_all(); // TODO apenas cancelar as transmissoes do nó que morreu
    for (auto& [id, t] : ab_transmissions) {
        if (id.origin == event.node.get_address()) ab_transmissions.erase(id);
    }

    if (!dispatcher.is_active()) dispatcher.cancel_all();
}

void BroadcastConnection::packet_received(const PacketReceived &event)
{
    if (!message_type::is_broadcast(event.packet.data.header.type)) return;
    if (event.packet.data.header.get_message_type() == MessageType::RAFT) return;

    Packet& packet = event.packet;

    if (packet.data.header.is_ack()) {
        receive_ack(packet);
        return;
    }

    uint32_t message_number = packet.data.header.id.msg_num;
    bool is_atomic = message_type::is_atomic(packet.data.header.get_message_type());

    const Node& origin = nodes.get_node(packet.data.header.id.origin);
    if (!sequence_numbers.contains(origin.get_id())) {
        log_warn("Received packet ", packet.to_string(PacketFormat::RECEIVED), ", but sequence number is not synchronized; sending RST.");
        send_rst(packet);
        return;
    }
    SequenceNumber& sequence = is_atomic ? ab_sequence_number : sequence_numbers.at(origin.get_id());

    if (message_number < sequence.initial_number)
    {
        log_debug(
            "Received ",
            packet.to_string(PacketFormat::RECEIVED),
            " which is prior to current connection with initial number ",
            sequence.initial_number,
            "; ignoring it."
        );
        return;
    }
    if (message_number > sequence.next_number)
    {
        if (!is_atomic)
        {
            log_debug(
            "Received ",
            packet.to_string(PacketFormat::RECEIVED),
            " that expects confirmation, but message number ",
            message_number,
            " is higher than the expected ",
            sequence.next_number,
            "; ignoring it."
            );
            return;
        }
        // TODO: testar isso, acho que o único jeito seria definindo manualmente no código os valores iniciais
        log_debug(
            "Received atomic broadcast with sequence number higher than the current one [",
            sequence.next_number,
            "]; ",
            "jumping atomic sequence number to ",
            message_number,
            "."
        );
        sequence.next_number = message_number;
    }

    if (packet.data.header.is_rst()) {
        Node& node = nodes.get_node(packet.meta.origin);
        log_info(
            "Received RST from ",
            node.to_string(),
            "; considering it as dead for the current broadcast.");
        pipeline.notify(NodeDeath(node));
        return;
    }

    receive_fragment(packet);
}

void BroadcastConnection::message_received(const MessageReceived &event)
{
    if (!message_type::is_broadcast(event.message.type)) return;
    
    const Message& message = event.message;
    const MessageIdentity& id = message.id;

    const Node& origin = nodes.get_node(message.id.origin);
    if (!sequence_numbers.contains(origin.get_id())) {
        log_warn("Received message ", message.to_string(), ", but sequence number is not synchronized.");
        return;
    };

    SequenceNumber& sequence = message_type::is_atomic(message.type) ? ab_sequence_number : sequence_numbers.at(origin.get_id());

    if (message.id.msg_num < sequence.next_number)
    {
        log_warn("Message ", message.to_string(), " was already received; dropping it.");
        return;
    }

    if (message.id.msg_num > sequence.next_number)
    {
        log_warn("Message ", message.to_string(), " is unexpected, current number expected is ", sequence.next_number, "; dropping it.");
        return;
    }

    sequence.next_number++;

    MessageType type = event.message.type;

    if (type == MessageType::BEB) {
        deliver_buffer.produce(event.message);
    }
    else if (message_type::is_urb(type)) {
        if (!retransmissions.contains(id)) {
            retransmissions.emplace(id, RetransmissionEntry());
        }
        RetransmissionEntry& entry = retransmissions.at(id);
        entry.message = event.message;
        entry.message_received = true;
        log_debug("Message from URB was received.", entry.received_all_acks ? "" : " Still waiting for all ACKs.");

        try_deliver(id);
    }
    else if (type == MessageType::AB_REQUEST) {
        if (!local_node.is_leader()) {
            log_warn("Received AB message from ", message.origin.to_string(), ", but local node is not the leader; ignoring it.");
            return;
        }

        if (ab_transmissions.contains(message.id)) return;

        log_info("Received AB message from ", message.origin.to_string(), ", retransmitting it.");

        ab_transmissions.emplace(message.id, std::make_shared<Transmission>(message, BROADCAST_ID));
        Transmission& t = *ab_transmissions.at(message.id);

        t.message.origin = local_node.get_address();
        t.message.destination = {BROADCAST_ADDRESS, 0};
        t.message.type = MessageType::AB_URB;

        ab_dispatcher.enqueue(t);
    }
}


void BroadcastConnection::receive_ack(Packet& ack_packet)
{
    const PacketHeader& header = ack_packet.data.header;
    const MessageIdentity& id = header.id;
    uint32_t frag_num = header.fragment_num;

    pipeline.notify(PacketAckReceived(ack_packet));

    if (message_type::is_urb(header.type)) {
        if (is_delivered(id, message_type::is_atomic(ack_packet.data.header.get_message_type()))) return;

        if (!retransmissions.contains(id)) retransmissions.emplace(id, RetransmissionEntry());
        RetransmissionEntry& entry = retransmissions.at(id);
        
        if (entry.retransmitted_fragments.contains(frag_num)) return;

        if (!entry.received_acks.contains(frag_num))
            entry.received_acks.emplace(frag_num, std::unordered_set<SocketAddress>());

        std::unordered_set<SocketAddress>& origins = entry.received_acks.at(frag_num);
        origins.emplace(ack_packet.meta.origin);
    }
}

void BroadcastConnection::receive_fragment(Packet& packet)
{
    const PacketHeader& header = packet.data.header;

    SocketAddress ack_destination = packet.meta.origin;

    if (message_type::is_urb(header.type)) {
        ack_destination = {BROADCAST_ADDRESS, 0};
        retransmit_fragment(packet);
    }

    Packet ack_packet = create_ack(packet, ack_destination);
    pipeline.send(ack_packet);
}

bool BroadcastConnection::is_delivered(const MessageIdentity& id, bool is_atomic) {
    if (!nodes.contains(id.origin)) return false;
    const Node& origin = nodes.get_node(id.origin);

    if (!sequence_numbers.contains(origin.get_id())) return false;
    const SequenceNumber& sequence = is_atomic ? ab_sequence_number : sequence_numbers.at(origin.get_id());

    return id.msg_num < sequence.next_number && !retransmissions.contains(id);
}

void BroadcastConnection::retransmit_fragment(Packet& packet)
{
    const PacketHeader& header = packet.data.header;
    const MessageIdentity& id = header.id;
    // TODO isso da problema no AB
    // if (id.origin == local_node.get_address()) return;

    if (is_delivered(id, message_type::is_atomic(packet.data.header.get_message_type()))) return;

    if (!retransmissions.contains(id)) {
        retransmissions.emplace(id, RetransmissionEntry());
    }
    RetransmissionEntry& entry = retransmissions.at(id);

    if (entry.retransmitted_fragments.contains(header.fragment_num)) return;
    entry.retransmitted_fragments.emplace(header.fragment_num);

    Packet rt_packet = packet;

    log_info("Received URB fragment ", rt_packet.to_string(PacketFormat::RECEIVED), ". Retransmitting back.");
    rt_packet.meta.transmission_uuid = entry.uuid;
    rt_packet.meta.destination = {BROADCAST_ADDRESS, 0};
    rt_packet.meta.expects_ack = true;
    rt_packet.meta.urb_retransmission = true;
    pipeline.send(rt_packet);

    if (entry.received_acks.contains(header.fragment_num)) {
        const std::unordered_set<SocketAddress>& origins = entry.received_acks.at(header.fragment_num);

        Packet ack_packet = packet;
        ack_packet.data.header.flags = ACK;

        for (const SocketAddress origin : origins) {
            ack_packet.meta.origin = origin;
            pipeline.notify(PacketAckReceived(ack_packet));
        }

        entry.received_acks.erase(header.fragment_num);
    }
}

void BroadcastConnection::try_deliver(const MessageIdentity& id) {
    if (!retransmissions.contains(id)) return;
    RetransmissionEntry& entry = retransmissions.at(id);

    if (entry.received_all_acks && entry.message_received) {
        log_debug("Delivering message ", entry.message.to_string(), ".");
        deliver_buffer.produce(entry.message);
        retransmissions.erase(id);
    }
}

void BroadcastConnection::transmission_complete(const TransmissionComplete& event) {
    const UUID &uuid = event.uuid;
    const MessageIdentity& id = event.id;

    const Transmission* ab_active = ab_dispatcher.get_active();
    if (ab_active && uuid == ab_active->uuid) ab_dispatcher.complete(true);
    if (ab_transmissions.contains(id)) ab_transmissions.erase(id);

    if (retransmissions.contains(event.id)) {
        RetransmissionEntry& entry = retransmissions.at(event.id);
        entry.received_all_acks = true;
        log_debug("All ACKs from URB retransmission were received.", entry.message_received ? "" : " Still missing message.");

        try_deliver(event.id);
    }

    const Transmission* active = dispatcher.get_active();
    if (active && uuid == active->uuid) dispatcher.complete(true);
}

void BroadcastConnection::transmission_fail(const TransmissionFail& event) {
    const Transmission* ab_active = ab_dispatcher.get_active();
    if (ab_active && event.uuid == ab_active->uuid) ab_dispatcher.complete(false);
    if (ab_transmissions.contains(event.id)) ab_transmissions.erase(event.id);

    const Transmission* active = dispatcher.get_active();
    if (!active) return;
    if (event.uuid != active->uuid) return;
    
    for (const Node* node : event.faulty_nodes) {
        if (!connections.contains(node->get_id())) continue;
        Connection& connection = connections.at(node->get_id());
        connection.close();
    }

    dispatcher.complete(false);
}

void BroadcastConnection::request_update() {
    connection_update_buffer.produce(BROADCAST_ID);
}

bool BroadcastConnection::enqueue(Transmission& transmission) {
    return dispatcher.enqueue(transmission);
}

bool BroadcastConnection::establish_all_connections() {
    bool established = true;

    for (auto& [node_id, connection] : connections) {
        const Node& node = nodes.get_node(node_id);
        if (!node.is_alive()) continue;

        ConnectionState state = connection.get_state();
 
        if (state != ConnectionState::ESTABLISHED) {
            established = false;
        }

        if (state == ConnectionState::CLOSED) {
            connection.connect();
        }
    }

    return established;
}

void BroadcastConnection::update() {
    send_dispatched_packets();

    ab_dispatcher.update();

    if (dispatcher.is_active()) return;

    Transmission* next_transmission = dispatcher.get_next();
    if (!next_transmission) return;

    bool established = establish_all_connections();
    if (!established) return;
 
    Message& message = next_transmission->message;
    if (message.type == MessageType::AB_REQUEST) {
        Node* leader = nodes.get_leader();

        if (!leader) {
            // TODO acho que nesse caso o RaftManager deve emitir um evento LeaderElected
            // em que o BroadcastConnection ouve e dá request_update.
            log_warn("No leader is elected right now. Atomic broadcast will hang.");
            return;
        }

        message.destination = leader->get_address();
    }

    dispatcher.update();
}

void BroadcastConnection::send_rst(Packet& packet)
{
    PacketHeader header = packet.data.header;
    header.flags = RST;

    PacketData data;
    memset(&data, 0, sizeof(PacketData));
    data.header = header;

    Packet rst_packet;
    rst_packet.meta.destination = packet.meta.origin;
    rst_packet.meta.message_length = 0;
    rst_packet.data = data;

    pipeline.send(rst_packet);
}

void BroadcastConnection::dispatch_to_sender(Packet packet)
{
    mutex_dispatched_packets.lock();
    dispatched_packets.push_back(packet);
    mutex_dispatched_packets.unlock();

    request_update();
}

void BroadcastConnection::send_dispatched_packets()
{
    mutex_dispatched_packets.lock();

    for (Packet p : dispatched_packets) pipeline.send(p);
    dispatched_packets.clear();

    mutex_dispatched_packets.unlock();
}
