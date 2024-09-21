#pragma once

#include <mutex>
#include <vector>
#include <unordered_set>
#include <map>

#include "channels/channel.h"
#include "core/packet.h"
#include "utils/date.h"
#include "pipeline/pipeline_handler.h"

struct QueueEntry {
    Packet packet;
    int timeout_id = -1;
    int tries = 0;
};

class TransmissionQueue
{
private:
    Timer& timer;
    PipelineHandler& handler;

    std::map<uint32_t, QueueEntry> entries;

    std::unordered_set<uint32_t> pending;
    uint32_t end_fragment_num = UINT32_MAX;

    std::mutex mutex_packets;
    std::mutex mutex_timeout;

    void reset();

    void send(uint32_t num);

    void timeout(uint32_t num);
public:
    TransmissionQueue(Timer& timer, PipelineHandler& handler);

    bool completed();

    void add_packet(const Packet& packet);

    void mark_acked(uint32_t num);
};
