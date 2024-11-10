#include "pipeline/fragmentation/fragmentation_layer.h"
#include <cmath>

using namespace std::placeholders;

FragmentationLayer::FragmentationLayer(PipelineHandler handler, EventBus &event_bus)
    : event_bus(event_bus), PipelineStep(handler)
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

    if (!packet.data.header.is_fragment())
        return;

    MessageIdentity& message_id = packet.data.header.id;

    mtx_assembler_map.lock();

    if (!assembler_map.contains(message_id))
        assembler_map.emplace(message_id, FragmentAssembler(event_bus));

    FragmentAssembler &assembler = assembler_map.at(message_id);
    assembler.add_packet(packet);

    mtx_assembler_map.unlock();

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
    obs_pipeline_cleanup.on(std::bind(&FragmentationLayer::pipeline_cleanup, this, _1));
    obs_node_death.on(std::bind(&FragmentationLayer::node_death, this, _1));
    bus.attach(obs_forward_defragmented_message);
    bus.attach(obs_pipeline_cleanup);
    bus.attach(obs_node_death);
}

void FragmentationLayer::forward_defragmented_message(const ForwardDefragmentedMessage &event)
{
    Packet &packet = event.packet;
    MessageIdentity& message_id = packet.data.header.id;

    mtx_assembler_map.lock();

    Message const message = assembler_map.at(message_id).assemble();
    assembler_map.erase(message_id);

    mtx_assembler_map.unlock();

    handler.forward_receive(message);
}

void FragmentationLayer::pipeline_cleanup(const PipelineCleanup &event)
{
    const MessageIdentity& id = event.id;

    mtx_assembler_map.lock();
    if (assembler_map.contains(id)) assembler_map.erase(id);
    mtx_assembler_map.unlock();
}

void FragmentationLayer::node_death(const NodeDeath &event)
{
    std::unordered_set<MessageIdentity> remove;

    mtx_assembler_map.lock();
    for (auto &[id, assembler] : assembler_map)
    {
        if (id.origin == event.remote_node.get_address() && !message_type::is_urb(id.msg_type)) remove.emplace(id);
    }
    if (remove.size() > 0) {
        log_debug("Cleaning ",
        remove.size(),
        " pending messages from ",
        event.remote_node.get_address().to_string(),
        " that were never received.");
    }
    for (const MessageIdentity &id : remove)
    {
        assembler_map.erase(id);
    }
    mtx_assembler_map.unlock();
}