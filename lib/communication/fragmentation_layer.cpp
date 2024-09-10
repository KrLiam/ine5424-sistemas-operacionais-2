#include "communication/fragmentation_layer.h"

FragmentationLayer::FragmentationLayer(Pipeline *control, ReliableCommunication *comm) : PipelineStep(PipelineStep::FRAGMENTATION_LAYER, control), comm(comm)
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

    Node destination = comm->get_node(message.destination);
    Connection *connection = comm->get_connection(destination.get_id());

    int required_packets = ceil((double)message.length / PacketData::MAX_MESSAGE_SIZE);
    log_debug("Message [", message.to_string(), "] length is ", message.length, "; will fragment into ", required_packets, " packets.");
    for (int i = 0; i < required_packets; i++)
    {
        PacketMetadata meta = {
            origin : comm->get_local_node().get_address(),
            destination : destination.get_address(),
            time : 0
        };
        PacketData data;
        bool more_fragments = i != required_packets - 1;
        data.header = {
            msg_num : connection->get_current_message_number(),
            fragment_num : i,
            type : message.type,
            ack : 0,
            more_fragments : more_fragments
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
        forward_send(pkt);
    }
}

void FragmentationLayer::receive(char *m)
{
    Packet packet;
    memcpy(&packet, m, sizeof(Packet));
    log_debug("Packet [", packet.to_string(), "] received on fragmentation layer.");

    Node origin = comm->get_node(packet.meta.origin);
    if (!assembler_map.contains(origin.get_id()))
    {
        assembler_map.emplace(origin.get_id(), FragmentAssembler());
    }
    FragmentAssembler assembler = assembler_map.at(origin.get_id());

    if (assembler.has_received(packet))
    {
        log_debug("Ignoring packet ", packet.to_string(), ", as it was already received.");
        return;
    }
    assembler.add_packet(packet);

    if (assembler.has_received_all_packets())
    {
        log_debug("Received all fragments; forwarding message to next step.");
        Message message = assembler.assemble();

        char msg[sizeof(Message)];
        memcpy(msg, &message, sizeof(Message));
        forward_receive(msg);
        assembler_map.erase(origin.get_id());
    }
}