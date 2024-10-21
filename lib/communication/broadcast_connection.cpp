
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
    dispatcher(BROADCAST_ID, connection_update_buffer, pipeline),
    connection_update_buffer(connection_update_buffer),
    deliver_buffer(deliver_buffer)
{
    observe_pipeline();
}

const TransmissionDispatcher& BroadcastConnection::get_dispatcher() const { return dispatcher; }

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
}
void BroadcastConnection::connection_established(const ConnectionEstablished&) {
    request_update();
}

void BroadcastConnection::connection_closed(const ConnectionClosed&) {
    if (!dispatcher.is_active()) dispatcher.cancel_all();
}

void BroadcastConnection::packet_received(const PacketReceived &event)
{
    if (!message_type::is_broadcast(event.packet.data.header.type)) return;
    if (event.packet.data.header.get_message_type() == MessageType::RAFT) return;

    Packet& packet = event.packet;
    uint32_t message_number = packet.data.header.id.msg_num;
    
    const Node& origin = nodes.get_node(packet.data.header.id.origin);
    if (!sequence_numbers.contains(origin.get_id())) {
        log_warn("Received packet ", packet.to_string(PacketFormat::RECEIVED), ", but sequence number is not synchronized; sending RST.");
        send_rst(packet);
        return;
    }
    SequenceNumber& sequence = sequence_numbers.at(origin.get_id());


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

    if (packet.data.header.is_rst()) {
        Node& node = nodes.get_node(packet.meta.origin);
        log_info(
            "Received RST from ",
            node.to_string(),
            "; considering it as dead for the current broadcast.");
        pipeline.notify(NodeDeath(node));
        return;
    }

    if (packet.data.header.is_ack()) {
        receive_ack(packet);
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
    SequenceNumber& sequence = sequence_numbers.at(origin.get_id());

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
    else if (type == MessageType::URB) {
        if (!retransmissions.contains(id)) {
            retransmissions.emplace(id, RetransmissionEntry());
        }
        RetransmissionEntry& entry = retransmissions.at(id);
        entry.message = event.message;
        entry.message_received = true;
        log_debug("Message from URB was received.", entry.received_all_acks ? "" : " Still waiting for all ACKs.");

        try_deliver(id);
    }
    else if (type == MessageType::AB) {
        // desconsiderar recebimento desta mensagem
        sequence.next_number--;

        if (!local_node.is_leader()) {
            log_warn("Received AB message from ", message.origin.to_string(), ", but local node is not the leader; ignoring it.");
            return;
        }

        log_info("Received AB message from ", message.origin.to_string(), ", retransmitting it.");
        // TODO provavelmente colocar a mensagem numa fila de transmissoes do AB, se não multiplos ABs acontecerão simultaneamente.
        // talvez dê pra manter um TransmissionDispatcher separado pra isso, mas daí essa instância não pode mudar o numero
        // da mensagem (na função activate), dá pra fazer o dispatcher receber uma flag no construtor de 'enumerate_messages' pra
        // desativar essa funcionalidade. daí o BroadcastConnection::update sempre dá update neste dispatcher.
        Message rt_message = message;
        rt_message.transmission_uuid = UUID();
        rt_message.origin = local_node.get_address();
        rt_message.destination = {BROADCAST_ADDRESS, 0};
        rt_message.type = MessageType::URB;
        pipeline.send(rt_message);
    }
}


void BroadcastConnection::receive_ack(Packet& ack_packet)
{
    const PacketHeader& header = ack_packet.data.header;
    const MessageIdentity& id = header.id;
    uint32_t frag_num = header.fragment_num;

    pipeline.notify(PacketAckReceived(ack_packet));

    if (header.type == MessageType::URB) {
        if (is_delivered(id)) return;

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

    if (header.type == MessageType::URB) {
        ack_destination = {BROADCAST_ADDRESS, 0};
        retransmit_fragment(packet);
    }

    Packet ack_packet = create_ack(packet, ack_destination);
    pipeline.send(ack_packet);
}

bool BroadcastConnection::is_delivered(const MessageIdentity& id) {
    if (!nodes.contains(id.origin)) return false;
    const Node& origin = nodes.get_node(id.origin);

    if (!sequence_numbers.contains(origin.get_id())) return false;
    const SequenceNumber& sequence = sequence_numbers.at(origin.get_id());

    return id.msg_num < sequence.next_number && !retransmissions.contains(id);
}

void BroadcastConnection::retransmit_fragment(Packet& packet)
{
    const PacketHeader& header = packet.data.header;
    const MessageIdentity& id = header.id; 
    if (id.origin == local_node.get_address()) return;

    if (is_delivered(id)) return;

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
    if (dispatcher.is_active()) return;

    Transmission* next_transmission = dispatcher.get_next();
    if (!next_transmission) return;

    bool established = establish_all_connections();
    if (!established) return;
 
    Message& message = next_transmission->message;
    if (message.type == MessageType::AB) {
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
