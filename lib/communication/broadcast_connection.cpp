
#include "communication/broadcast_connection.h"

RetransmissionEntry::RetransmissionEntry() {}

BroadcastConnection::BroadcastConnection(
    NodeMap& nodes,
    Node& local_node,
    std::map<std::string, Connection>& connections,
    BufferSet<std::string>& connection_update_buffer,
    Buffer<Message>& deliver_buffer,
    Pipeline& pipeline
) :
    nodes(nodes),
    local_node(local_node),
    connections(connections),
    connection_update_buffer(connection_update_buffer),
    deliver_buffer(deliver_buffer),
    pipeline(pipeline),
    ab_dispatcher(BROADCAST_ID, connection_update_buffer, pipeline),
    dispatcher(BROADCAST_ID, connection_update_buffer, pipeline),
    ab_sequence_number({0, 0}),
    ab_next_deliver(0),
    delayed_ab_number(0)
{
    observe_pipeline();
}

const TransmissionDispatcher& BroadcastConnection::get_dispatcher() const { return dispatcher; }

const TransmissionDispatcher& BroadcastConnection::get_ab_dispatcher() const { return ab_dispatcher; }

void BroadcastConnection::observe_pipeline() {
    obs_receive_synchronization.on(std::bind(&BroadcastConnection::receive_synchronization, this, _1));
    obs_connection_established.on(std::bind(&BroadcastConnection::connection_established, this, _1));
    // obs_connection_closed.on(std::bind(&BroadcastConnection::connection_closed, this, _1));
    obs_packet_received.on(std::bind(&BroadcastConnection::packet_received, this, _1));
    obs_message_received.on(std::bind(&BroadcastConnection::message_received, this, _1));
    obs_unicast_message_received.on(std::bind(&BroadcastConnection::unicast_message_received, this, _1));
    obs_transmission_fail.on(std::bind(&BroadcastConnection::transmission_fail, this, _1));
    obs_transmission_complete.on(std::bind(&BroadcastConnection::transmission_complete, this, _1));
    obs_atomic_mapping.on(std::bind(&BroadcastConnection::atomic_mapping, this, _1));
    obs_node_up.on(std::bind(&BroadcastConnection::node_up, this, _1));
    pipeline.attach(obs_receive_synchronization);
    pipeline.attach(obs_connection_established);
    // pipeline.attach(obs_connection_closed);
    pipeline.attach(obs_packet_received);
    pipeline.attach(obs_message_received);
    pipeline.attach(obs_unicast_message_received);
    pipeline.attach(obs_transmission_complete);
    pipeline.attach(obs_transmission_fail);
    pipeline.attach(obs_atomic_mapping);
    pipeline.attach(obs_node_up);
}

SequenceNumber* BroadcastConnection::get_sequence(const MessageIdentity& id) {
    if (message_type::is_atomic(id.msg_type)) return &ab_sequence_number;

    if (!nodes.contains(id.origin)) return nullptr;
    const Node& origin = nodes.get_node(id.origin);

    if (!sequence_numbers.contains(origin.get_id())) return nullptr;
    return &sequence_numbers.at(origin.get_id());
}

void BroadcastConnection::receive_synchronization(const ReceiveSynchronization& event) {
    const std::string& id = event.node.get_id();

    if (!sequence_numbers.contains(id)) sequence_numbers.emplace(id, SequenceNumber());
    SequenceNumber& number = sequence_numbers.at(id);
    number.initial_number = 0;
    number.next_number = event.expected_broadcast_number;
    log_print(local_node.get_id(), " Broadcast sequence with node ", id, " was resync to ", number.next_number);

    if (ab_sequence_number.next_number >= event.expected_ab_number) return;
    if (ab_dispatcher.is_active())
    {
        log_warn(
            "Cannot change the expected AB number while we have an active transmission; ", 
            "we will synchronize after it finishes."
            );
        if (event.expected_ab_number > delayed_ab_number) delayed_ab_number = event.expected_ab_number;
        return;
    }
    synchronize_ab_number(event.expected_ab_number);
}
void BroadcastConnection::connection_established(const ConnectionEstablished&) {
    request_update();
}

void BroadcastConnection::node_death(const NodeDeath& event) {
    if (event.remote_node.is_leader()) {
        std::unordered_set<MessageIdentity> remove;

        for (auto& [id, entry] : retransmissions) {
            if (!message_type::is_atomic(entry.message.id.msg_type)) continue;
            if (entry.message_received) continue;

            log_warn("AB message ", entry.message.to_string().c_str(), " is incomplete and leader is dead; advancing sequence.");
            remove.emplace(id);
            synchronize_ab_number(ab_sequence_number.next_number + 1);
        }

        for (const MessageIdentity& id : remove) {
            retransmissions.erase(id);
        }
    }
}

void BroadcastConnection::node_up(const NodeUp&) {
    request_update();
}

void BroadcastConnection::atomic_mapping(const AtomicMapping& event) {
    if (!atomic_to_request_map.contains(event.atomic_id))
        atomic_to_request_map.emplace(event.atomic_id, event.request_id);
}

/*
void BroadcastConnection::connection_closed(const ConnectionClosed& event) {
    ab_dispatcher.cancel_all(); // TODO apenas cancelar as transmissoes do nó que morreu

    std::unordered_set<MessageIdentity> remove;
    for (auto& [id, t] : ab_transmissions) {
        if (id.origin == event.node.get_address()) remove.emplace(id);
    }
    for (const MessageIdentity& id : remove) {
        ab_transmissions.erase(id);
    }

    if (!dispatcher.is_active()) dispatcher.cancel_all();
}
*/

void BroadcastConnection::packet_received(const PacketReceived &event)
{
    if (!message_type::is_broadcast(event.packet.data.header.get_message_type())) return;
    if (event.packet.data.header.get_message_type() == MessageType::RAFT) return;

    Packet& packet = event.packet;

    if (packet.data.header.is_ack()) {
        log_print("received ack ", packet.data.header.id.msg_num);
        receive_ack(packet);
        return;
    }
    log_print(local_node.get_id(), " received ", packet.data.header.id.msg_num, " from ", packet.meta.origin.to_string());

    if (packet.data.header.is_rst()) {
        Node& node = nodes.get_node(packet.meta.origin);
        log_print(
            "Received RST from ",
            node.to_string(),
            "; considering it as dead for the current broadcast.");
        pipeline.notify(NodeDeath(node));
        try_deliver(packet.data.header.id);
        return;
    }

    uint32_t message_number = packet.data.header.id.msg_num;
    bool is_atomic = message_type::is_atomic(packet.data.header.get_message_type());

    SequenceNumber* sequence = get_sequence(packet.data.header.id);
    log_print(local_node.get_id(), " Received ", message_number, " from ", packet.data.header.id.origin.to_string(), " is at ", sequence->next_number, " ", sequence->initial_number, " ", ab_next_deliver);
    if (!sequence) {
        log_print("Received packet ", packet.to_string(PacketFormat::RECEIVED), ", but sequence number is not synchronized; sending RST.");
        send_rst(packet);
        return;
    }

    if (message_number < sequence->initial_number)
    {
        log_print(
            "Received ",
            packet.to_string(PacketFormat::RECEIVED),
            " which is prior to current connection with initial number ",
            sequence->initial_number,
            "; ignoring it."
        );
        send_rst(packet);
        return;
    }
    if (message_number > sequence->next_number)
    {
        /*if (!is_atomic)
        {
            log_debug(
            "Received ",
            packet.to_string(PacketFormat::RECEIVED),
            " that expects confirmation, but message number ",
            message_number,
            " is higher than the expected ",
            sequence->next_number,
            "; ignoring it."
            );
            return;
        }*/
        log_print(
            "Received ",
            is_atomic ? "atomic broadcast" : "broadcast",
            " with sequence number higher than the current one [",
            sequence->next_number,
            "]; ",
            "jumping sequence number to ",
            message_number,
            "."
        );
        sequence->next_number = message_number;
        if (is_atomic && !has_pending_ab_delivery()) ab_next_deliver = message_number;  // Se tem uma entrega AB pendente, o ab_next_deliver vai pular no try_deliver_next_atomic()
    }

    receive_fragment(packet);
}

void BroadcastConnection::message_received(const MessageReceived &event)
{
    if (!message_type::is_broadcast(event.message.id.msg_type)) return;
    
    const Message& message = event.message;
    const MessageIdentity& id = message.id;

    SequenceNumber* sequence = get_sequence(message.id);
    if (!sequence) {
        log_warn("Received message ", message.to_string(), ", but sequence number is not synchronized.");
        return;
    };

    if (message.id.msg_num < sequence->next_number)
    {
        log_warn("Message ", message.to_string(), " was already received; dropping it.");
        return;
    }

    if (message.id.msg_num > sequence->next_number)
    {
        log_warn("Message ", message.to_string(), " is unexpected, current number expected is ", sequence->next_number, "; dropping it.");
        return;
    }

    sequence->next_number++;
    log_print(local_node.get_id(), " incrementing seq number to ", sequence->next_number);
    if (message_type::is_atomic(message.id.msg_type))
    {
        ab_dispatcher.reset_number(sequence->next_number);
        local_node.set_receiving_ab_broadcast(false);
    }
    

    MessageType type = event.message.id.msg_type;

    if (type == MessageType::BEB) {
        if (event.message.is_application()) deliver_buffer.produce(event.message);
    }
    else if (message_type::is_urb(type)) {
        if (!retransmissions.contains(id)) {
            retransmissions.emplace(id, RetransmissionEntry());
        }
        RetransmissionEntry& entry = retransmissions.at(id);
        entry.message = event.message;
        entry.message_received = true;
        log_print("Message from URB was received.", entry.received_all_acks ? "" : " Still waiting for all ACKs.");

        try_deliver(id);
    }
}

void BroadcastConnection::unicast_message_received(const UnicastMessageReceived &event)
{
    const Message& message = event.message;
    MessageType type = message.id.msg_type;

    if (type == MessageType::AB_REQUEST) {
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
        t.message.id.msg_type = MessageType::AB_URB;

        ab_dispatcher.enqueue(t);
    }
}


void BroadcastConnection::receive_ack(Packet& ack_packet)
{
    const PacketHeader& header = ack_packet.data.header;
    const MessageIdentity& id = header.id;
    uint32_t frag_num = header.fragment_num;

    pipeline.notify(PacketAckReceived(ack_packet));

    if (message_type::is_urb(header.get_message_type())) {
        if (is_delivered(id, message_type::is_atomic(header.get_message_type()))) return;

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

    if (message_type::is_atomic(header.get_message_type()) && header.id.msg_num == ab_sequence_number.next_number) local_node.set_receiving_ab_broadcast(true);

    SocketAddress ack_destination = packet.meta.origin;

    if (message_type::is_urb(header.get_message_type())) {
        ack_destination = {BROADCAST_ADDRESS, 0};
        retransmit_fragment(packet);
    }

    log_print(local_node.get_id(), " SENDING ACK ", packet.data.header.id.msg_num);
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

    const Transmission* active = dispatcher.get_active();
    const Transmission* ab_active = ab_dispatcher.get_active();
    if (active && id == active->message.id) return;
    if (ab_active && id == ab_active->message.id) return;

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

    bool is_atomic = message_type::is_atomic(id.msg_type);
    if (is_atomic && id.msg_num != ab_next_deliver) return;

    RetransmissionEntry& entry = retransmissions.at(id);
    log_print(local_node.get_id(), "trying to deliver ", id.msg_num, " ", entry.message.to_string(), " ", entry.received_all_acks, entry.message_received);
    if (!entry.received_all_acks || !entry.message_received) return;

    if (entry.message.is_application())
    {
        log_print(local_node.get_id(), " Delivering message ", entry.message.to_string(), ".");
        deliver_buffer.produce(entry.message);
    }
    retransmissions.erase(id);

    if (is_atomic) try_deliver_next_atomic();
}

void BroadcastConnection::try_deliver_next_atomic()
{
    MessageIdentity next_atomic = {
        origin : {{0, 0, 0, 0}, 0},
        msg_num : UINT32_MAX,
        msg_type : (MessageType)0
    };

    for (auto& [key, entry] : retransmissions)
    {
        if (!message_type::is_atomic(entry.message.id.msg_type)) continue;
        uint32_t num = key.msg_num;
        if (num > ab_next_deliver && next_atomic.msg_num > num) {
            next_atomic = key;
        }
    }

    if (next_atomic.msg_num != UINT32_MAX)
    {
        ab_next_deliver = next_atomic.msg_num;
        try_deliver(next_atomic);
        return;
    }
    ab_next_deliver++;  // Se tiver salto, vai ser ressincronizado no receive
}

void BroadcastConnection::transmission_complete(const TransmissionComplete& event) {
    if (!message_type::is_broadcast(event.id.msg_type)) return;
    log_print("transmission of complete ", event.id.msg_num);

    const MessageIdentity& id = event.id;
    const UUID &uuid = event.uuid;

    if (retransmissions.contains(id)) {
        RetransmissionEntry& entry = retransmissions.at(id);
        entry.received_all_acks = true;
        log_debug("All ACKs from URB retransmission were received.", entry.message_received ? "" : " Still missing message.");

        try_deliver(id);
    }

    const Transmission* ab_active = ab_dispatcher.get_active();
    if (ab_active && uuid == ab_active->uuid)
    {
        ab_dispatcher.complete(true);
        if (atomic_to_request_map.contains(id)) {
            const MessageIdentity& ab_id = atomic_to_request_map.at(id);
            if (ab_transmissions.contains(ab_id)) ab_transmissions.erase(ab_id);
        }
        if (delayed_ab_number > ab_sequence_number.next_number) synchronize_ab_number(delayed_ab_number);
    }

    const Transmission* active = dispatcher.get_active();
    if (active && uuid == active->uuid) dispatcher.complete(true);
}

void BroadcastConnection::transmission_fail(const TransmissionFail& event) {
    const Transmission* ab_active = ab_dispatcher.get_active();
    if (ab_active && event.uuid == ab_active->uuid) ab_dispatcher.complete(false);
    if (atomic_to_request_map.contains(event.id)) {
        const MessageIdentity& ab_id = atomic_to_request_map.at(event.id);
        if (ab_transmissions.contains(ab_id)) ab_transmissions.erase(ab_id);
    }

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

bool BroadcastConnection::establish_all_connections(const std::unordered_set<std::string>& node_ids) {
    bool established = true;
    //log_print(local_node.get_id(), " ", node_ids.size());

    for (auto& node_id : node_ids) {
        //log_print(local_node.get_id(), " ", node_id);
        Connection& connection = connections.at(node_id);

        const Node& node = nodes.get_node(node_id);
        if (node.get_state() == NOT_INITIALIZED) {
            log_warn("Node ", node.get_id(), " is not initialized; broadcast will hang.");
            established = false;
            continue;
        }
        if (node.get_state() == FAULTY) continue;

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

    if (ab_dispatcher.is_empty() && dispatcher.is_empty()) return;
    if (ab_dispatcher.is_active() && dispatcher.is_active()) return;

    std::unordered_set<std::string> node_ids;
    for (auto& [id, node] : nodes) {
        node_ids.emplace(node.get_id());
    }
    if (Transmission* t = dispatcher.get_next()) {
        uint64_t group_hash = t->message.group_hash;
        for (Node* node : nodes.get_group(group_hash)) {
            // log_print(local_node.get_id(), " will ", node->get_id());
            node_ids.emplace(node->get_id());
        }
    }
    if (Transmission* t = ab_dispatcher.get_next()) {
        uint64_t group_hash = t->message.group_hash;
        for (Node* node : nodes.get_group(group_hash)) node_ids.emplace(node->get_id());
    }
    
    bool established = establish_all_connections(node_ids);
    if (!established) return;
    log_print(local_node.get_id(), " established connection with everyone");

    ab_dispatcher.update();
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

void BroadcastConnection::synchronize_ab_number(uint32_t number)
{
    ab_sequence_number.initial_number = number;
    ab_sequence_number.next_number = number;
    ab_next_deliver = number;
    ab_dispatcher.reset_number(number);
    log_info("Atomic broadcast number synchronized to ", number, ".");
}

bool BroadcastConnection::has_pending_ab_delivery()
{
    for (auto& [_, entry] : retransmissions)
    {
        if (!message_type::is_atomic(entry.message.id.msg_type)) return true;
    }
    return false;
}
