#include "pipeline/transmission/transmission_queue.h"
#include "core/constants.h"
#include "core/event.h"

TransmissionQueue::TransmissionQueue(Timer& timer, PipelineHandler& handler)
    : timer(timer), handler(handler)
{
}

void TransmissionQueue::send(uint32_t num) {
    QueueEntry& entry = entries.at(num);

    handler.forward_send(entry.packet);
    entry.tries++;

    pending.emplace(num);
    entry.timeout_id = timer.add(ACK_TIMEOUT, [this, num]() { timeout(num); });
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

    if (entry.tries > MAX_PACKET_TRIES)
    {
        log_error("Packet [", packet.to_string(PacketFormat::SENT), "] expired. Transmission failed.");

        handler.notify(TransmissionFail(entry.packet));

        reset();

        mutex_timeout.unlock();
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
    
    entries.emplace(num, QueueEntry{packet : packet});
    send(num);

    if (packet.data.header.is_end())
    {
        end_fragment_num = num;
    }
}

void TransmissionQueue::receive_ack(const Packet& ack_packet)
{
    uint32_t msg_num = ack_packet.data.header.get_message_number();
    uint32_t frag_num = ack_packet.data.header.get_fragment_number();

    if (msg_num != message_num) return;

    if (!entries.contains(frag_num)) return;
    QueueEntry& entry = entries.at(frag_num);
    
    const Packet& packet = entry.packet;

    if (entry.timeout_id != -1)
    {
        log_debug("Cancelling timer of packet ", message_num, "/", frag_num, ".");
        timer.cancel(entry.timeout_id);
        entry.timeout_id = -1;
    }

    pending.erase(frag_num);

    mutex_timeout.lock();
    if (completed()) {
        const UUID& uuid = packet.meta.transmission_uuid;
        SocketAddress remote_address = packet.meta.destination;
        uint32_t msg_num = packet.data.header.get_message_number();
        TransmissionComplete event(uuid, remote_address, msg_num);

        log_info(
            "Transmission ", remote_address.to_string(), " / ", msg_num, " is completed. Sent ",
            end_fragment_num + 1, " fragments, ", get_total_bytes(), " bytes total."
        );

        reset();

        handler.notify(event);
    }
    mutex_timeout.unlock();
}
