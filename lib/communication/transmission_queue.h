#pragma once

#include <vector>

#include "channels/channel.h"
#include "core/segment.h"
#include "utils/date.h"

class TransmissionQueue
{
private:
    std::vector<uint32_t> fragments_awaiting_ack = std::vector<uint32_t>{};
    std::vector<Packet> fragments_to_send = std::vector<Packet>{};

public:
    TransmissionQueue() {}

    bool has_sent_everything()
    {
        return fragments_to_send.size() == 0;
    }

    void add_packet_to_queue(Packet packet)
    {
        fragments_to_send.push_back(packet);
    }

    void mark_packet_as_acked(Packet packet)
    {
        uint32_t fragment_number = (uint32_t)packet.data.header.fragment_num;
        fragments_awaiting_ack.erase(std::remove(fragments_awaiting_ack.begin(), fragments_awaiting_ack.end(), fragment_number), fragments_awaiting_ack.end());
        // fragments_to_send.erase(std::remove(fragments_to_send.begin(), fragments_to_send.end(), packet), fragments_to_send.end());
    }

    void send_timedout_packets(Channel *channel)
    {
        unsigned int now = DateUtils::now();
        for (Packet &packet : fragments_to_send)
        {
            if (now - packet.meta.time < ACK_TIMEOUT)
            {
                continue;
            }
            channel->send(packet);
            packet.meta.time = now;
        }
    }
};
