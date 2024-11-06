#include "pipeline/transmission/transmission_layer.h"

using namespace std::placeholders;

bool TransmissionKey::operator==(const TransmissionKey& other) const {
    return id == other.id
        && destination == other.destination;
}

TransmissionLayer::TransmissionLayer(PipelineHandler handler, NodeMap &nodes)
    : PipelineStep(handler), nodes(nodes)
{
}

TransmissionLayer::~TransmissionLayer()
{
}

TransmissionKey TransmissionLayer::create_key(const MessageIdentity& id, const SocketAddress& remote) {
    return {
        id,
        message_type::is_broadcast(id.msg_type) ? SocketAddress{BROADCAST_ADDRESS, 0} : remote
    };
}

bool TransmissionLayer::has_queue(const TransmissionKey& key) {
    return queue_map.contains(key);
}
TransmissionQueue& TransmissionLayer::get_queue(const TransmissionKey& key) {
    if (!has_queue(key)) {
        auto queue = std::make_shared<TransmissionQueue>(handler, nodes);

        queue_map.insert({key, queue});
        // log_info("Added queue to ", id.origin.to_string(), " ", id.msg_num, " ", id.sequence_type);
    }

    return *queue_map.at(key);
}
void TransmissionLayer::clear_queue(const TransmissionKey& key) {
    queue_map.erase(key);
    // log_info("Cleared queue to ", id.origin.to_string(), " ", id.msg_num, " ", id.sequence_type);

}

void TransmissionLayer::attach(EventBus& bus) {
    obs_ack_received.on(std::bind(&TransmissionLayer::ack_received, this, _1));
    bus.attach(obs_ack_received);
    obs_pipeline_cleanup.on(std::bind(&TransmissionLayer::pipeline_cleanup, this, _1));
    bus.attach(obs_pipeline_cleanup);
    obs_node_death.on(std::bind(&TransmissionLayer::node_death, this, _1));
    bus.attach(obs_node_death);
}

void TransmissionLayer::send(Packet packet)
{
    const PacketMetadata& meta = packet.meta;
    const PacketHeader& header = packet.data.header;

    if (!packet.silent()) {
        log_trace("Packet [", packet.to_string(PacketFormat::SENT), "] sent to transmission layer.");
    }

    if (!packet.meta.expects_ack)
    {
        if (!packet.silent()) {
            log_debug("Packet [", packet.to_string(PacketFormat::SENT), "] does not require ACK, sending forward.");
        }
        handler.forward_send(packet);
        return;
    }

    TransmissionKey key = create_key(header.id, meta.destination);
    TransmissionQueue& queue = get_queue(key);
    queue.add_packet(packet);
}

void TransmissionLayer::ack_received(const PacketAckReceived& event) {    
    Packet& packet = event.ack_packet;

    const MessageIdentity& id = packet.data.header.id;
    TransmissionKey key = create_key(id, packet.meta.origin);

    if (has_queue(key)) {
        TransmissionQueue& queue = get_queue(key);
        queue.receive_ack(packet);
    }
}

void TransmissionLayer::pipeline_cleanup(const PipelineCleanup& event) {    
    clear_queue(create_key(event.id, event.destination));
}

void TransmissionLayer::node_death(const NodeDeath& event) {
    for (auto& [_, queue] : queue_map) {
        queue->discard_node(event.remote_node);
    }
}

void TransmissionLayer::receive(Packet packet)
{
    if (!packet.silent()) {
        log_trace("Packet [", packet.to_string(PacketFormat::RECEIVED), "] received on transmission layer.");
    }

    if (!nodes.contains(packet.meta.origin))
    {
        log_debug("Packet ", packet.to_string(PacketFormat::RECEIVED), " does not originate from group; dropping.");
        return;
    }

    handler.forward_receive(packet);
}
