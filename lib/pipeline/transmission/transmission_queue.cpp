#include "pipeline/transmission/transmission_queue.h"
#include "core/constants.h"
#include "core/event.h"


QueueEntry::QueueEntry(const Packet& packet) : packet(packet) {}


TransmissionQueue::TransmissionQueue(Timer& timer, PipelineHandler& handler, NodeMap& nodes)
    : timer(timer), handler(handler), nodes(nodes)
{
}

TransmissionQueue::~TransmissionQueue() {
    for (auto& [_, entry] : entries) {
        if (entry.timeout_id != -1) timer.cancel(entry.timeout_id);
    }
}

void TransmissionQueue::send(uint32_t num) {
    QueueEntry& entry = entries.at(num);
    Packet& packet = entry.packet;
    const SocketAddress& receiver_address = packet.meta.destination;

    if (entry.tries == 0) {
        if (receiver_address == BROADCAST_ADDRESS) {
            for (const auto& [_, node] : nodes) {
                if (node.get_address() == BROADCAST_ADDRESS) continue;
                if (!node.is_alive()) continue;
                entry.pending_receivers.emplace(&node);
            }
        }
        else {
            if (!nodes.contains(receiver_address)) {
                log_warn(
                    "Cannot transmit ",
                    packet.to_string(PacketFormat::SENT),
                    " because destination is unknown."
                );
                fail();
                return;
            } 
            const Node& receiver = nodes.get_node(receiver_address);
            if (!receiver.is_alive())
            {
                log_warn("Cannot transmit ",
                    packet.to_string(PacketFormat::SENT),
                    " because destination is dead.");
                fail();
                return;
            }
            entry.pending_receivers.emplace(&receiver);
        }
    }

    entry.tries++;
    pending.emplace(num);
    entry.timeout_id = timer.add(ACK_TIMEOUT, [this, num]() { timeout(num); });

    // não transmite o fragmento na primeira tentativa
    if (packet.meta.urb_retransmission && entry.tries == 1) return;

    Packet p = packet;

    // evitar que a receiver thread apague um receiver enquanto a timeout thread tenta enviar para ele
    mutex_timeout.lock();

    for (const Node* receiver : entry.pending_receivers) {
        p.meta.destination = receiver->get_address();
        handler.forward_send(p);
    }

    mutex_timeout.unlock();
}

void TransmissionQueue::fail()
{
    std::unordered_set<const Packet*> faulty_packets;
    std::unordered_set<const Node*> faulty_receivers;

    for (auto& [_, entry] : entries) {
        if (entry.pending_receivers.empty()) continue;

        faulty_packets.emplace(&entry.packet);

        for (const Node* receiver : entry.pending_receivers) {
            faulty_receivers.emplace(receiver);
        }
    }

    const QueueEntry& first_entry = entries.begin()->second;
    const UUID uuid = first_entry.packet.meta.transmission_uuid;

    log_error(
        "Transmission failed with ",
        faulty_receivers.size(),
        " faulty node(s) and ",
        faulty_packets.size(),
        " unreceived packet(s)."
    );

    reset();
    handler.notify(TransmissionFail(uuid, faulty_packets, faulty_receivers));
}

void TransmissionQueue::timeout(uint32_t num)
{
    mutex_timeout.lock();

    if (!entries.contains(num))
    {
        mutex_timeout.unlock();
        return;
    };

    QueueEntry& entry = entries.at(num);
    Packet& packet = entry.packet;

    if (entry.pending_receivers.empty()) {
        pending.erase(num);
        mutex_timeout.unlock();
        try_complete();
        return;
    }

    if (entry.tries > MAX_PACKET_TRIES)
    {
        mutex_timeout.unlock();
        fail();
        return;
    }

    mutex_timeout.unlock();

    log_warn("Packet [", packet.to_string(PacketFormat::SENT), "] timed out. Sending again, already tried ", entry.tries, " time(s).");
    send(num);
}

void TransmissionQueue::reset() {
    for (auto& pair : entries) {
        QueueEntry& entry = pair.second;

        if (entry.timeout_id != -1)
        {
            timer.cancel(entry.timeout_id);
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
    
    entries.emplace(num, QueueEntry(packet));
    send(num);

    if (packet.data.header.is_end())
    {
        end_fragment_num = num;
    }
}

bool TransmissionQueue::try_complete()
{
    mutex_timeout.lock();

    if (entries.empty()) return false;

    bool success = completed();
    if (success) {
        const QueueEntry& entry = entries.begin()->second;
        const Packet& packet = entry.packet;
        const UUID& uuid = packet.meta.transmission_uuid;
        SocketAddress remote_address = packet.meta.destination;
        uint32_t msg_num = packet.data.header.get_message_number();
        TransmissionComplete event(uuid, packet.data.header.id, remote_address);

        log_info(
            "Transmission ", remote_address.to_string(), " / ", msg_num, " is completed. Sent ",
            end_fragment_num + 1, " fragments, ", get_total_bytes(), " bytes total."
        );

        reset();

        handler.notify(event);
    }

    mutex_timeout.unlock();

    return success;
}

void TransmissionQueue::receive_ack(const Packet& ack_packet)
{
    uint32_t msg_num = ack_packet.data.header.get_message_number();
    uint32_t frag_num = ack_packet.data.header.get_fragment_number();
    const SocketAddress& receiver_address = ack_packet.meta.origin;

    if (msg_num != message_num) return;

    if (!entries.contains(frag_num)) return;
    QueueEntry& entry = entries.at(frag_num);

    if (!nodes.contains(receiver_address)) return;
    const Node& receiver = nodes.get_node(receiver_address);

    // evitar que a receiver thread apague um receiver enquanto a timeout thread tenta enviar para ele
    mutex_timeout.lock();
    entry.pending_receivers.erase(&receiver);
    mutex_timeout.unlock();

    // log_info("Removing pending ack ", frag_num, " of remote ", receiver_address.to_string(), ". over: ", !entry.pending_receivers.size());
    if (entry.pending_receivers.size()) return;

    pending.erase(frag_num);
    // log_info("Completed: ", completed());

    if (entry.timeout_id != -1)
    {
        timer.cancel(entry.timeout_id);
        entry.timeout_id = -1;
    }

    try_complete();
}

void TransmissionQueue::discard_node(const Node& node) {
    for (auto& [_, entry] : entries) {
        if (!message_type::is_broadcast(entry.packet.data.header.type)) continue;

        entry.pending_receivers.erase(&node);

        if (entry.pending_receivers.empty())
            pending.erase(entry.packet.data.header.fragment_num);
    }
    
    try_complete();
}
