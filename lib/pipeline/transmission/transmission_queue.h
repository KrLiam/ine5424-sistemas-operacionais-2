#pragma once

#include <mutex>
#include <vector>
#include <unordered_set>
#include <map>

#include "core/packet.h"
#include "core/node.h"
#include "utils/date.h"
#include "pipeline/pipeline_handler.h"

struct QueueEntry {
    Packet packet;
    int timeout_id = -1;
    int tries = 0;
    std::unordered_set<SocketAddress> pending_receivers;

    QueueEntry(const Packet& packet);
};


class TransmissionQueue
{
private:
    Timer& timer;
    PipelineHandler& handler;
    const NodeMap& nodes;

    std::map<uint32_t, QueueEntry> entries;
    std::unordered_set<uint32_t> pending;

    uint32_t message_num = UINT32_MAX;
    uint32_t end_fragment_num = UINT32_MAX;

    std::mutex mutex_packets;
    std::mutex mutex_timeout;

    void send(uint32_t num);

    void timeout(uint32_t num);

    bool try_complete();
public:
    TransmissionQueue(Timer& timer, PipelineHandler& handler, const NodeMap& nodes);
    ~TransmissionQueue();

    uint32_t get_total_bytes();

    bool completed();

    void add_packet(const Packet& packet);

    void receive_ack(const Packet& packet);

    void discard_node(const Node& node);

    void reset();
};
