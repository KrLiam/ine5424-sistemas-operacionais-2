#include "pipeline/transmission/transmission_layer.h"

TransmissionLayer::TransmissionLayer(PipelineHandler handler, const NodeMap &nodes)
    : PipelineStep(handler), nodes(nodes)
{
}

TransmissionLayer::~TransmissionLayer()
{
}

TransmissionQueue& TransmissionLayer::get_queue(const std::string& id) {
    if (!queue_map.contains(id))
        queue_map.insert({id, std::make_unique<TransmissionQueue>(timer, handler)});
    return *queue_map.at(id);
}

void TransmissionLayer::attach(EventBus& bus) {
    obs_ack_received.on(std::bind(&TransmissionLayer::ack_received, this, _1));
    bus.attach(obs_ack_received);
    obs_pipeline_cleanup.on(std::bind(&TransmissionLayer::pipeline_cleanup, this, _1));
    bus.attach(obs_pipeline_cleanup);
}

void TransmissionLayer::send(Packet packet)
{
    log_trace("Packet [", packet.to_string(PacketFormat::SENT), "] sent to transmission layer.");

    if (!packet.meta.expects_ack)
    {
        log_debug("Packet [", packet.to_string(PacketFormat::SENT), "] does not require ACK, sending forward.");
        handler.forward_send(packet);
        return;
    }

    const Node& destination = nodes.get_node(packet.meta.destination);
    const std::string& id = destination.get_id();
    TransmissionQueue& queue = get_queue(id);
    queue.add_packet(packet);
}

void TransmissionLayer::ack_received(const PacketAckReceived& event) {    
    Packet& packet = event.ack_packet;

    const Node& origin = nodes.get_node(packet.meta.origin);
    const std::string& id = origin.get_id();
    TransmissionQueue& queue = get_queue(id);

    queue.receive_ack(packet);
}

void TransmissionLayer::pipeline_cleanup(const PipelineCleanup& event) {    
    Message& message = event.message;

    const Node& origin = nodes.get_node(message.destination);
    const std::string& id = origin.get_id();
    TransmissionQueue& queue = get_queue(id);

    queue.reset();
}

void TransmissionLayer::receive(Packet packet)
{
    log_trace("Packet [", packet.to_string(PacketFormat::RECEIVED), "] received on transmission layer.");

    if (!nodes.contains(packet.meta.origin))
    {
        log_debug("Packet ", packet.to_string(PacketFormat::RECEIVED), " does not originate from group; dropping.");
        return;
    }

    handler.forward_receive(packet);
}
