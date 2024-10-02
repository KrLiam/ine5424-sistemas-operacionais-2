#pragma once

#include <unordered_map>
#include <thread>
#include <atomic>
#include <memory>

#include "pipeline/pipeline_step.h"
#include "pipeline/transmission/transmission_queue.h"
#include "core/node.h"
#include "core/message.h"
#include "core/packet.h"
#include "core/buffer.h"

class TransmissionLayer : public PipelineStep
{
private:
    Timer timer;
    const NodeMap &nodes;

    std::unordered_map<std::string, std::unique_ptr<TransmissionQueue>> queue_map;

    Observer<PacketAckReceived> obs_ack_received;
    void ack_received(const PacketAckReceived& event);
    Observer<PipelineCleanup> obs_pipeline_cleanup;
    void pipeline_cleanup(const PipelineCleanup& event);

    TransmissionQueue& get_queue(const std::string& id);

public:
    TransmissionLayer(PipelineHandler handler, const NodeMap &nodes);
    ~TransmissionLayer() override;

    void attach(EventBus&);

    void send(Packet packet);
    void receive(Packet packet);
};
