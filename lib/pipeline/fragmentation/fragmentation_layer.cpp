#include "pipeline/fragmentation/fragmentation_layer.h"

FragmentationLayer::FragmentationLayer(PipelineHandler handler, GroupRegistry *gr)
    : PipelineStep(handler, gr)
{
}

FragmentationLayer::~FragmentationLayer()
= default;

void FragmentationLayer::service()
{
}

void FragmentationLayer::send(Message message)
{
    log_debug("Message [", message.to_string(), "] sent to fragmentation layer.");

    Node destination = gr->get_node(message.destination);

    uint32_t required_packets = ceil((double)message.length / PacketData::MAX_MESSAGE_SIZE);
    log_debug("Message [", message.to_string(), "] length is ", message.length, "; will fragment into ", required_packets, " packets.");
    // Mutex para não ter duas threads usando o mesmo msg_num simultaneamente
    for (uint32_t i = 0; i < required_packets; i++)
    {
        PacketMetadata meta = {
            origin : gr->get_local_node().get_address(),
            destination : destination.get_address(),
            time : 0,
            message_length : static_cast<int>(message.length),
        };
        PacketData data{};
        bool more_fragments = i != required_packets - 1;
        data.header = { // TODO: Definir checksum, window e reserved corretamente
            msg_num : message.number,
            fragment_num : i,
            checksum : 0,
            window : 0,
            ack : 0,
            rst : 0,
            syn : 0,
            fin : 0,
            extra : 0,
            more_fragments : more_fragments,
            type : message.type,
            reserved : 0
        };
        meta.message_length = std::min((int)(message.length - i * PacketData::MAX_MESSAGE_SIZE), (int)PacketData::MAX_MESSAGE_SIZE);
        strncpy(data.message_data, &message.data[i * PacketData::MAX_MESSAGE_SIZE], meta.message_length);
        Packet packet = {
            data : data,
            meta : meta
        };

        log_debug("Forwarding packet ", packet.to_string(), " to next step.");
        handler.forward_send(packet);
    }
}

void FragmentationLayer::send(Packet packet)
{
    log_debug("Packet [", packet.to_string(), "] sent to fragmentation layer.");
    handler.forward_send(packet);
}

void FragmentationLayer::receive(Packet packet)
{
    log_debug("Packet [", packet.to_string(), "] received on fragmentation layer.");

    if (packet.data.header.type == MessageType::APPLICATION)
    {
        Node origin = gr->get_node(packet.meta.origin);
        if (!assembler_map.contains(origin.get_id()))
        {
            assembler_map.emplace(origin.get_id(), FragmentAssembler());
        }
        FragmentAssembler &assembler = assembler_map.at(origin.get_id());

        if (assembler.has_received(packet))
        {
            log_debug("Ignoring packet ", packet.to_string(), ", as it was already received.");
            return;
        }
        assembler.add_packet(packet);
    }

    handler.forward_receive(packet);

    /*if (assembler.has_received_all_packets())
    {
        log_debug("Received all fragments; forwarding message to next step.");
        Message message = assembler.assemble();
        assembler_map.erase(origin.get_id());
        handler.forward_receive(message);
    }*/
}