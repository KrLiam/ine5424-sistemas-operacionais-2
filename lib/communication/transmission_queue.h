#pragma once

#include <vector>

#include "channels/channel.h"
#include "core/segment.h"
#include "utils/date.h"

class TransmissionQueue
{
private:
    std::vector<uint32_t> fragments_awaiting_ack;
    std::vector<Packet> fragments_to_send;

public:
    TransmissionQueue();

    bool has_sent_everything();
    void add_packet_to_queue(Packet packet);
    void mark_packet_as_acked(Packet packet);
    void send_timedout_packets(Channel *channel);
};
