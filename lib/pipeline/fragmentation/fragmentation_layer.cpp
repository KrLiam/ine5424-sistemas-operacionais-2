#include "pipeline/fragmentation/fragmentation_layer.h"
#include <cmath>

using namespace std::placeholders;

FragmentationLayer::FragmentationLayer(PipelineHandler handler)
    : PipelineStep(handler)
{
}

FragmentationLayer::~FragmentationLayer() = default;

void FragmentationLayer::send(Message message)
{
    Fragmenter fragmenter(message);

    log_debug("Message [", message.to_string(), "] length is ", message.length, "; will fragment into ", fragmenter.get_total_fragments(), " packets.");

    Packet packet;
    while (fragmenter.has_next())
    {
        fragmenter.next(&packet);
        log_trace("Forwarding ", packet.to_string(PacketFormat::SENT), " to next step.");
        handler.forward_send(packet);
    }
}

void FragmentationLayer::send(Packet packet)
{
    if (!packet.silent()) {
        log_trace("Packet [", packet.to_string(PacketFormat::SENT), "] sent to fragmentation layer.");
    }
    handler.forward_send(packet);
}

void FragmentationLayer::receive(Packet packet)
{
    if (!packet.silent()) {
        log_trace("Packet [", packet.to_string(PacketFormat::RECEIVED), "] received on fragmentation layer.");
    }
    handler.forward_receive(packet);

    if (!message_type::is_application(packet.data.header.get_message_type()))
        return;

    std::string message_id = get_message_identifier(packet);

    if (!assembler_map.contains(message_id))
        assembler_map.emplace(message_id, FragmentAssembler());

    FragmentAssembler &assembler = assembler_map.at(message_id);
    assembler.add_packet(packet);

    if (!assembler.is_complete())
        return;

    if (!packet.silent()) {
        log_debug("Received all fragments; notifying connection.");
    }
    handler.notify(MessageDefragmentationIsComplete(packet));
}

void FragmentationLayer::attach(EventBus &bus)
{
    obs_forward_defragmented_message.on(std::bind(&FragmentationLayer::forward_defragmented_message, this, _1));
    bus.attach(obs_forward_defragmented_message);
}

void FragmentationLayer::forward_defragmented_message(const ForwardDefragmentedMessage &event)
{
    Packet &packet = event.packet;
    std::string message_id = get_message_identifier(packet);

    Message const message = assembler_map.at(message_id).assemble();
    assembler_map.erase(message_id);

    handler.forward_receive(message);
}