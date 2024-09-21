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

    log_debug("Sending packet [", entry.packet.to_string(), "], try number ", entry.tries, ". Setting timer.");

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

    if (entry.tries >= MAX_PACKET_TRIES)
    {
        log_debug("Packet [", packet.to_string(), "] expired. Transmission failed.");

        TransmissionFail event(entry.packet);
        reset();

        handler.notify(event);

        mutex_timeout.unlock();
        return;
    }

    mutex_timeout.unlock();

    log_debug("Packet [", packet.to_string(), "] timed out. Resending.");
    send(num);
}

void TransmissionQueue::reset() {
    for (const auto& pair : entries) {
        const QueueEntry& entry = pair.second;
        timer.cancel(entry.timeout_id);
    }

    pending.clear();
    entries.clear();
    end_fragment_num = UINT32_MAX;
}

bool TransmissionQueue::completed()
{
    return !pending.size() && end_fragment_num != UINT32_MAX;
}

void TransmissionQueue::add_packet(const Packet& packet)
{
    uint32_t num = packet.data.header.get_fragment_number();
    
    entries.emplace(num, QueueEntry{packet : packet});
    send(num);

    if (packet.data.header.is_end())
    {
        end_fragment_num = num;
    }
}

void TransmissionQueue::mark_acked(uint32_t num)
{
    QueueEntry& entry = entries.at(num);
    const Packet& packet = entry.packet;

    timer.cancel(entry.timeout_id);
    pending.erase(num);

    if (completed()) {
        SocketAddress remote_address = packet.meta.destination;
        uint32_t msg_num = packet.data.header.get_message_number();
        TransmissionComplete event(remote_address, msg_num);

        log_debug("Transmission ", remote_address.to_string(), " / ", msg_num, " is completed.");

        reset();

        handler.notify(event);
        return;
    }

    log_debug("Received ack for packet [", packet.to_string(), "].");
}
