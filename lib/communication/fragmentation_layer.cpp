#include "communication/fragmentation_layer.h"

FragmentationLayer::FragmentationLayer(PipelineHandler handler, GroupRegistry &gr)
    : PipelineStep(handler, gr)
{
}

FragmentationLayer::~FragmentationLayer()
{
}

void FragmentationLayer::service()
{
}

void FragmentationLayer::send(char *m)
{
    Message message;
    memcpy(&message, m, sizeof(Message));
    log_debug("Message [", message.to_string(), "] sent to fragmentation layer.");

    Node destination = gr.get_node(message.destination);
    Connection *connection = gr.get_connection(destination.get_id());

    int required_packets = ceil((double)message.length / PacketData::MAX_MESSAGE_SIZE);
    log_debug("Message [", message.to_string(), "] length is ", message.length, "; will fragment into ", required_packets, " packets.");
    for (int i = 0; i < required_packets; i++)
    {
        PacketMetadata meta = {
            origin : gr.get_local_node().get_address(),
            destination : destination.get_address(),
            time : 0,
            message_length : static_cast<int>(message.length),
        };
        PacketData data;
        bool more_fragments = i != required_packets - 1;
        data.header = { // TODO: Definir checksum, window e reserved corretamente
            msg_num : connection->get_current_message_number(),
            fragment_num : i,
            checksum : 0,
            window : 0,
            type : message.type,
            ack : 0,
            more_fragments : more_fragments,
            reserved : 0
        };
        meta.message_length = std::min((int)(message.length - i * PacketData::MAX_MESSAGE_SIZE), (int)PacketData::MAX_MESSAGE_SIZE);
        strncpy(data.message_data, &message.data[i * PacketData::MAX_MESSAGE_SIZE], meta.message_length);
        Packet packet = {
            data : data,
            meta : meta
        };

        log_debug("Forwarding packet ", packet.to_string(), " to next step.");
        char pkt[sizeof(Packet)];
        memcpy(pkt, &packet, sizeof(Packet));
        handler.forward_send(pkt);
    }
}

void FragmentationLayer::receive(char *m)
{
    Packet packet;
    memcpy(&packet, m, sizeof(Packet));
    log_debug("Packet [", packet.to_string(), "] received on fragmentation layer.");

    Node origin = gr.get_node(packet.meta.origin);
    if (!assembler_map.contains(origin.get_id()))
    {
        assembler_map.emplace(origin.get_id(), new FragmentAssembler());
    }
    FragmentAssembler *assembler = assembler_map.at(origin.get_id());

    if (assembler->has_received(packet))
    {
        log_debug("Ignoring packet ", packet.to_string(), ", as it was already received.");
        return;
    }
    assembler->add_packet(packet);

    if (assembler->has_received_all_packets())
    {
        log_debug("Received all fragments; forwarding message to next step.");
        Message message = assembler->assemble();

        char msg[sizeof(Message)];
        memcpy(msg, &message, sizeof(Message));
        handler.forward_receive(msg);
        delete assembler;
        assembler_map.erase(origin.get_id());
    }
}