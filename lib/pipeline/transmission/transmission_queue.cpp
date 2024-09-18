#include "pipeline/transmission/transmission_queue.h"

TransmissionQueue::TransmissionQueue() : fragments_awaiting_ack(), fragments_to_send()
{
}

bool TransmissionQueue::has_sent_everything()
{
    queue_mutex.lock();
    bool result = fragments_to_send.size() == 0;
    queue_mutex.unlock();
    return result;
}

void TransmissionQueue::add_packet_to_queue(Packet packet)
{
    queue_mutex.lock();
    fragments_to_send.push_back(packet);
    queue_mutex.unlock();
}

void TransmissionQueue::mark_packet_as_acked(Packet packet)
{
    uint32_t fragment_number = (uint32_t)packet.data.header.fragment_num;
    fragments_awaiting_ack.erase(std::remove(fragments_awaiting_ack.begin(), fragments_awaiting_ack.end(), fragment_number), fragments_awaiting_ack.end());
    // fragments_to_send.erase(std::remove(fragments_to_send.begin(), fragments_to_send.end(), packet), fragments_to_send.end());
    // TODO: achar um jeito melhor de remover o fragmento da lista de fragmentos pra enviar
    queue_mutex.lock();
    for (std::size_t i = 0; i < fragments_to_send.size(); i++)
    {
        Packet p = fragments_to_send[i];
        if (p.data.header.fragment_num == packet.data.header.fragment_num)
        {
            fragments_to_send.erase(fragments_to_send.begin() + i);
            break;
        }
    }
    queue_mutex.unlock();
}

void TransmissionQueue::send_timedout_packets(Channel *channel)
{
    uint64_t now = DateUtils::now();
    std::vector<std::size_t> indexes_to_remove = std::vector<std::size_t>();

    queue_mutex.lock();
    for (std::size_t i = 0; i < fragments_to_send.size(); i++)
    {
        Packet packet = fragments_to_send[i];

        if (now - packet.meta.time < ACK_TIMEOUT)
            continue;
        channel->send(packet);

        bool is_ack = packet.data.header.ack;
        if (is_ack)
        {
            indexes_to_remove.push_back(i);
            continue;
        }

        bool is_control = packet.data.header.type == MessageType::CONTROL;
        if (is_control)
        {
            indexes_to_remove.push_back(i);
            continue;
        }

        fragments_to_send[i].meta.time = now;
        fragments_awaiting_ack.push_back((uint32_t)packet.data.header.fragment_num); // melhor usar set pra isso
    }

    int offset = 0;
    for (int idx : indexes_to_remove)
    {
        fragments_to_send.erase(fragments_to_send.begin() + idx + offset);
        offset--;
    }
    queue_mutex.unlock();
}