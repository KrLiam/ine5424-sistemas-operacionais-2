#pragma once

#include <mutex>
#include <vector>

#include "channels/channel.h"
#include "core/packet.h"
#include "utils/date.h"

class TransmissionQueue
{
private:
    std::vector<uint32_t> fragments_awaiting_ack;
    std::vector<Packet> fragments_to_send;

    std::mutex queue_mutex;

public:
    TransmissionQueue();

    bool has_sent_everything();
    /**
     * Used by the received thread to insert a packet into the send queue.
    */
    void add_packet_to_queue(Packet packet);
    /**
     * Used by the sender thread to remove a packet from the pending ACK list and the send queue.
    */
    void mark_packet_as_acked(Packet packet);
    /**
     * Used by the sender thread to send packets from the send queue.
    */
    void send_timedout_packets(Channel *channel);
};
