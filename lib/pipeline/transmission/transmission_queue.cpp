#include "pipeline/transmission/transmission_queue.h"
#include "core/constants.h"
#include "core/event.h"


QueueEntry::QueueEntry(const Packet& packet) : packet(packet) {}


TransmissionQueue::TransmissionQueue(PipelineHandler& handler, NodeMap& nodes)
    : handler(handler), nodes(nodes), destroyed(false), timeout_dis(Config::ACK_TIMEOUT, Config::ACK_TIMEOUT)
{
}

TransmissionQueue::~TransmissionQueue() {
    destroyed = true;
    mutex_timeout.lock();
    for (auto& [_, entry] : entries) {
        if (entry.timeout_id != -1) TIMER.cancel(entry.timeout_id);
    }
    mutex_timeout.unlock();
}

void TransmissionQueue::send(uint32_t num) {
    mutex_timeout.lock();

    if (!entries.contains(num)) {
        mutex_timeout.unlock();
        return;
    }

    QueueEntry& entry = entries.at(num);
    Packet& packet = entry.packet;
    const SocketAddress& receiver_address = packet.meta.destination;

    if (entry.tries == 0) {
        if (receiver_address == BROADCAST_ADDRESS) {
            uint64_t group_hash = packet.data.header.key_hash;
            // log_info(group_hash, " ", nodes.get_group(group_hash).size(), " ", nodes.size());
            for (const Node* node : nodes.get_group(group_hash)) {
                if (node->get_address() == BROADCAST_ADDRESS) continue;
                if (!node->is_alive()) continue;
                entry.pending_receivers.emplace(node);
                log_print(packet.meta.origin.to_string(), " on group there is ", node->get_id(), " to receive ", packet.data.header.id.msg_num);
            }
        }
        else {
            if (!nodes.contains(receiver_address)) {
                log_warn(
                    "Cannot transmit ",
                    packet.to_string(PacketFormat::SENT),
                    " because destination is unknown."
                );
                mutex_timeout.unlock();
                fail();
                return;
            } 
            const Node& receiver = nodes.get_node(receiver_address);
            /*if (receiver.get_state())
            {
                log_warn("Cannot transmit ",
                    packet.to_string(PacketFormat::SENT),
                    " because destination is dead.");
                mutex_timeout.unlock();
                fail();
                return;
            }*/
            entry.pending_receivers.emplace(&receiver);
        }
    }

    entry.tries++;
    pending.emplace(num);
    if (entry.timeout_id != -1) {
        TIMER.cancel(entry.timeout_id);
        entry.timeout_id = -1;
    }
    entry.timeout_id = TIMER.add(timeout_dis(rc_random::gen), [this, num]() { timeout(num); });

    // não transmite o fragmento na primeira tentativa
    if (packet.meta.urb_retransmission && entry.tries == 1) {
        mutex_timeout.unlock();
        return;
    }

    Packet p = packet;

    for (const Node* receiver : entry.pending_receivers) {
        if (packet.data.header.get_message_type() == MessageType::URB && !packet.data.header.is_ack()) {
            log_print(packet.meta.origin.to_string(), " sending ack of ", packet.data.header.id.msg_num, " to ", receiver->get_address().to_string());
        }
        p.meta.destination = receiver->get_address();
        handler.forward_send(p);
    }

    mutex_timeout.unlock();
}

void TransmissionQueue::fail()
{
    std::unordered_set<const Packet*> faulty_packets;
    std::unordered_set<const Node*> faulty_receivers;

    mutex_timeout.lock();

    for (auto& [_, entry] : entries) {
        if (entry.pending_receivers.empty()) continue;

        faulty_packets.emplace(&entry.packet);

        for (const Node* receiver : entry.pending_receivers) {
            faulty_receivers.emplace(receiver);
        }
    }

    const QueueEntry& first_entry = entries.begin()->second;
    const UUID uuid = first_entry.packet.meta.transmission_uuid;
    const MessageIdentity id = first_entry.packet.data.header.id;

    log_error(
        first_entry.packet.data.header.id.origin.to_string(),
        " Transmission failed with ",
        faulty_receivers.size(),
        " faulty node(s) and ",
        faulty_packets.size(),
        " unreceived packet(s)."
    );
    for (auto& receiver : faulty_receivers) {
        log_print(receiver->get_id());
    }

    reset();
    mutex_timeout.unlock();

    handler.notify(TransmissionFail(uuid, id, faulty_packets, faulty_receivers));
}

void TransmissionQueue::timeout(uint32_t num)
{
    log_print(" Packet [", num, "] timed out");
    if (destroyed) return;
    mutex_timeout.lock();

    if (!entries.contains(num))
    {
        log_print("a");
        mutex_timeout.unlock();
        return;
    };

    QueueEntry& entry = entries.at(num);
    const Packet& packet = entry.packet;

    if (entry.pending_receivers.empty()) {
        log_print("b");
        pending.erase(num);
        mutex_timeout.unlock();
        try_complete();
        return;
    }

    if (packet_can_timeout(packet) && entry.tries > MAX_PACKET_TRIES)
    {
        mutex_timeout.unlock();
        fail();
        return;
    }

    mutex_timeout.unlock();

    log_print(packet.meta.origin.to_string(), " Packet [", packet.to_string(PacketFormat::SENT), "] timed out. Sending again, already tried ", entry.tries, " time(s).");
    send(num);
}

void TransmissionQueue::reset() {
    for (auto& pair : entries) {
        QueueEntry& entry = pair.second;

        if (entry.timeout_id != -1)
        {
            TIMER.cancel(entry.timeout_id);
            entry.timeout_id = -1;
        }
    }

    pending.clear();
    entries.clear();
    message_num = UINT32_MAX;
    end_fragment_num = UINT32_MAX;
}

uint32_t TransmissionQueue::get_total_bytes() {
    uint32_t sum = 0;
    for (const auto& pair : entries) {
        const QueueEntry& entry = pair.second;
        sum += entry.packet.meta.message_length;
    }
    return sum;
}

bool TransmissionQueue::completed()
{
    return !pending.size() && end_fragment_num != UINT32_MAX;
}

void TransmissionQueue::add_packet(const Packet& packet)
{
    uint32_t msg_num = packet.data.header.get_message_number();
    uint32_t num = packet.data.header.get_fragment_number();

    if (message_num == UINT32_MAX)
    {
        message_num = msg_num;
    }
    else if (message_num != msg_num)
    {
        log_warn(
            "Transmission queue received packet with message number of ",
            msg_num,
            " during transmission of message of number ",
            message_num,
            ". Ignoring it."
        );
        return;
    }
    
    mutex_timeout.lock();
    entries.emplace(num, QueueEntry(packet));
    mutex_timeout.unlock();

    send(num);

    if (packet.data.header.is_end())
    {
        end_fragment_num = num;
    }
}

bool TransmissionQueue::try_complete()
{
    mutex_timeout.lock();

    if (entries.empty())
    {
        mutex_timeout.unlock();
        return false;
    }

    log_info("trying to complete ", entries.begin()->second.packet.data.header.id.msg_num);
    bool success = completed();
    if (success) {
        const QueueEntry& entry = entries.begin()->second;
        const Packet& packet = entry.packet;
        const UUID& uuid = packet.meta.transmission_uuid;
        SocketAddress remote_address = packet.meta.destination;
        [[maybe_unused]] uint32_t msg_num = packet.data.header.get_message_number();
        TransmissionComplete event(uuid, packet.data.header.id, packet.data.header.id.origin, remote_address);

        log_info(
            "Transmission ", remote_address.to_string(), " / ", msg_num, " is completed. Sent ",
            end_fragment_num + 1, " fragments, ", get_total_bytes(), " bytes total."
        );
        PipelineCleanup e(packet.data.header.id, {BROADCAST_ADDRESS, 0});

        reset();

        handler.notify(event);
        // TIMER.add(0, [this, e]() { handler.notify(e); });        
    }
    mutex_timeout.unlock();

    return success;
}

void TransmissionQueue::receive_ack(const Packet& ack_packet)
{
    uint32_t msg_num = ack_packet.data.header.get_message_number();
    uint32_t frag_num = ack_packet.data.header.get_fragment_number();
    const SocketAddress& receiver_address = ack_packet.meta.origin;
    log_print(ack_packet.meta.destination.to_string(), " receive ack q", msg_num, " ", message_num, " ", ack_packet.data.header.id.origin.to_string(), " ", ack_packet.meta.origin.to_string());

    if (msg_num != message_num) return;
    log_print("a", msg_num);

    mutex_timeout.lock();

    if (!entries.contains(frag_num)) {
        mutex_timeout.unlock();
        log_print("c", msg_num);
        return;
    }
    QueueEntry& entry = entries.at(frag_num);

    if (!nodes.contains(receiver_address)) {
        mutex_timeout.unlock();
        log_print("b", msg_num);
        return;
    }
    const Node& receiver = nodes.get_node(receiver_address);

    entry.pending_receivers.erase(&receiver);

    // log_info("Removing pending ack ", frag_num, " of remote ", receiver_address.to_string(), ". over: ", !entry.pending_receivers.size());
    if (entry.pending_receivers.size()) {
        mutex_timeout.unlock();
        log_print("d", msg_num);
        for (const Node* node : entry.pending_receivers) {
            log_print(ack_packet.meta.destination.to_string(), " is waiting ack from ", node->get_id());
        }
        return;
    }

    pending.erase(frag_num);
    // log_info("Completed: ", completed());

    if (entry.timeout_id != -1)
    {
        TIMER.cancel(entry.timeout_id);
        entry.timeout_id = -1;
    }
    mutex_timeout.unlock();

    try_complete();
}

void TransmissionQueue::discard_node(const Node& node) {
    mutex_timeout.lock();
    for (auto& [_, entry] : entries) {
        if (!message_type::is_broadcast(entry.packet.data.header.get_message_type())) continue;

        entry.pending_receivers.erase(&node);


        if (entry.pending_receivers.empty())
            pending.erase(entry.packet.data.header.fragment_num);
    }
    mutex_timeout.unlock();
    
    try_complete();
}

bool TransmissionQueue::packet_can_timeout(const Packet& packet)
{
    return !message_type::is_urb(packet.data.header.get_message_type());
}
