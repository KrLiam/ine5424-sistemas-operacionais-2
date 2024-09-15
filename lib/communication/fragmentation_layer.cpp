#include "communication/fragmentation_layer.h"
#include "communication/fragmenter.h"

FragmentationLayer::FragmentationLayer(PipelineHandler handler, GroupRegistry *gr)
    : PipelineStep(handler, gr)
{
}

FragmentationLayer::~FragmentationLayer()
{
}

void FragmentationLayer::service()
{
}

void FragmentationLayer::send(Message message)
{
    Fragmenter fragmenter(message);

    log_debug("Message [", message.to_string(), "] length is ", message.length, "; will fragment into ", fragmenter.get_total_fragments(), " packets.");

    Packet packet;
    while (fragmenter.has_next())
    {
        fragmenter.next(&packet);

        log_debug("Forwarding packet ", packet.to_string(), " to next step.");
        handler.forward_send(packet);
    }
}

FragmentAssembler& FragmentationLayer::get_assembler(const std::string& id) {
    assembler_map_mutex.lock();

    if (!assembler_map.contains(id))
    {
        assembler_map.emplace(id, FragmentAssembler());
    }
    FragmentAssembler& assembler = assembler_map.at(id);

    assembler_map_mutex.unlock();

    return assembler;
}

void FragmentationLayer::discard_assembler(const std::string& id) {
    assembler_map_mutex.lock();
    assembler_map.erase(id);
    assembler_map_mutex.unlock();
}

void FragmentationLayer::receive(Packet packet)
{
    if (packet.data.header.type != MessageType::DATA) {
        handler.forward_receive(packet);
        return;
    }

    log_debug("Packet [", packet.to_string(), "] received on fragmentation layer.");

    Node origin = gr->get_node(packet.meta.origin);
    FragmentAssembler& assembler = get_assembler(origin.get_id());

    bool added = assembler.add_packet(packet);
    if (!added) return;

    if (assembler.is_complete())
    {
        log_debug("Received all fragments; forwarding message to next step.");

        Message message = assembler.assemble();
        discard_assembler(origin.get_id());

        handler.forward_receive(message);
    }
}