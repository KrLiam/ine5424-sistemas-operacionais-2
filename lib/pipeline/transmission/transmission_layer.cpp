#include "pipeline/transmission/transmission_layer.h"

TransmissionLayer::TransmissionLayer(PipelineHandler handler, GroupRegistry *gr, Channel* channel) : PipelineStep(handler, gr), channel(channel)
{
    service();
}

TransmissionLayer::~TransmissionLayer()
{
    channel->shutdown_socket();
}


const std::string& TransmissionLayer::get_id(const Packet& packet) {
    Node destination = gr->get_node(packet.meta.destination);
    return destination.get_id();
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
    log_trace("Packet [", packet.to_string(), "] sent to transmission layer.");

    if (!packet.meta.expects_ack)
    {
        log_debug("Packet [", packet.to_string(), "] does not require transmission, sending forward.");
        handler.forward_send(packet);
        return;
    }

    const std::string& id = get_id(packet);
    TransmissionQueue& queue = get_queue(id);
    queue.add_packet(packet);
}

void TransmissionLayer::ack_received(const PacketAckReceived& event) {    
    Packet& packet = event.ack_packet;

    const std::string& id = get_id(packet);
    TransmissionQueue& queue = get_queue(id);

    uint32_t num = packet.data.header.get_fragment_number();
    queue.mark_acked(num);
}


void TransmissionLayer::receive(Packet packet)
{
    log_trace("Packet [", packet.to_string(), "] received on transmission layer.");

    handler.forward_receive(packet);
}
