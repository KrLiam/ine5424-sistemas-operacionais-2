#include "pipeline/transmission/transmission_layer.h"

TransmissionLayer::TransmissionLayer(PipelineHandler handler, GroupRegistry *gr, Channel* channel) : PipelineStep(handler, gr), channel(channel)
{
}

TransmissionLayer::~TransmissionLayer()
{
}

TransmissionQueue& TransmissionLayer::get_queue(const std::string& id) {
    if (!queue_map.contains(id))
    {
        queue_map.insert({id, std::make_unique<TransmissionQueue>(timer, handler)});
    }

    return *queue_map.at(id);
}

void TransmissionLayer::attach(EventBus& bus) {
    obs_ack_received.on(std::bind(&TransmissionLayer::ack_received, this, _1));
    bus.attach(obs_ack_received);
}

void TransmissionLayer::send(Packet packet)
{
    log_trace("Packet [", packet.to_string(PacketFormat::SENT), "] sent to transmission layer.");

    if (!packet.meta.expects_ack)
    {
        log_debug("Packet [", packet.to_string(PacketFormat::SENT), "] does not require ack, sending forward.");
        handler.forward_send(packet);
        return;
    }

    const Node& destination = gr->get_node(packet.meta.destination);
    const std::string& id = destination.get_id();
    TransmissionQueue& queue = get_queue(id);
    queue.add_packet(packet);
}

void TransmissionLayer::ack_received(const PacketAckReceived& event) {    
    Packet& packet = event.ack_packet;

    const Node& origin = gr->get_node(packet.meta.origin);
    const std::string& id = origin.get_id();
    TransmissionQueue& queue = get_queue(id);

    queue.receive_ack(packet);
}


void TransmissionLayer::receive(Packet packet)
{
    log_trace("Packet [", packet.to_string(PacketFormat::RECEIVED), "] received on transmission layer.");

    handler.forward_receive(packet);
}
