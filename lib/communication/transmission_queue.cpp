#include "transmission_queue.h"

TransmissionQueue::TransmissionQueue() : fragments_awaiting_ack(), fragments_to_send()
{
}

bool TransmissionQueue::has_sent_everything()
{
    return fragments_to_send.size() == 0;
}

void TransmissionQueue::add_packet_to_queue(Packet packet)
{
    fragments_to_send.push_back(packet);
}

void TransmissionQueue::mark_packet_as_acked(Packet packet)
{
    // controlar com mtx
    uint32_t fragment_number = (uint32_t)packet.data.header.fragment_num;
    fragments_awaiting_ack.erase(std::remove(fragments_awaiting_ack.begin(), fragments_awaiting_ack.end(), fragment_number), fragments_awaiting_ack.end());
    // fragments_to_send.erase(std::remove(fragments_to_send.begin(), fragments_to_send.end(), packet), fragments_to_send.end());
    // TODO: achar um jeito melhor de remover o fragmento da lista de fragmentos pra enviar
    for (std::size_t i = 0; i < fragments_to_send.size(); i++)
    {
        Packet p = fragments_to_send[i];
        if (p.data.header.fragment_num == packet.data.header.fragment_num)
        {
            fragments_to_send.erase(fragments_to_send.begin() + i);
            break;
        }
    }
}

void TransmissionQueue::send_timedout_packets(Channel *channel)
{
    // mtx
    unsigned int now = DateUtils::now();
    for (Packet &packet : fragments_to_send)
    {
        if (now - packet.meta.time < ACK_TIMEOUT)
        {
            continue;
        }
        channel->send(packet);
        packet.meta.time = now;
        fragments_awaiting_ack.push_back((uint32_t)packet.data.header.fragment_num); // melhor usar set pra isso
    }
}