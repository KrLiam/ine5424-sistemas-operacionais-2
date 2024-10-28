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

struct TransmissionKey {
    MessageIdentity id;
    SocketAddress destination;

    bool operator==(const TransmissionKey& other) const;
};

template<> struct std::hash<TransmissionKey> {
    std::size_t operator()(const TransmissionKey& key) const {
        return std::hash<MessageIdentity>()(key.id)
            ^ std::hash<SocketAddress>()(key.destination);
    }
};


class TransmissionLayer : public PipelineStep
{
private:
    Timer timer;
    NodeMap &nodes;

    std::unordered_map<TransmissionKey, std::shared_ptr<TransmissionQueue>> queue_map;

    Observer<PacketAckReceived> obs_ack_received;
    void ack_received(const PacketAckReceived& event);
    Observer<PipelineCleanup> obs_pipeline_cleanup;
    void pipeline_cleanup(const PipelineCleanup& event);
    Observer<NodeDeath> obs_node_death;
    void node_death(const NodeDeath& event);

    TransmissionKey create_key(const MessageIdentity& id, const SocketAddress& remote);
    bool has_queue(const TransmissionKey& id);
    TransmissionQueue& get_queue(const TransmissionKey& id);
    void clear_queue(const TransmissionKey& id);

public:
    TransmissionLayer(PipelineHandler handler, NodeMap &nodes);
    ~TransmissionLayer() override;

    void attach(EventBus&);

    void send(Packet packet);
    void receive(Packet packet);
};
