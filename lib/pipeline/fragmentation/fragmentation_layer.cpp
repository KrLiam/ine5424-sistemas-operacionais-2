#include "pipeline/fragmentation/fragmentation_layer.h"
#include <cmath>

FragmentationLayer::FragmentationLayer(PipelineHandler handler, GroupRegistry *gr)
    : PipelineStep(handler, gr)
{
}

FragmentationLayer::~FragmentationLayer() = default;

void FragmentationLayer::service()
{
}

void FragmentationLayer::send(Message message)
{
    log_trace("Message [", message.to_string(), "] sent to fragmentation layer.");

    Node destination = gr->get_node(message.destination);

    uint32_t required_packets = std::ceil(static_cast<double>(message.length) / PacketData::MAX_MESSAGE_SIZE);
    log_trace("Message [", message.to_string(), "] length is ", message.length, "; will fragment into ", required_packets, " packets.");
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
        data.header = {// TODO: Definir checksum, window e reserved corretamente
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

        log_trace("Forwarding packet ", packet.to_string(), " to next step.");
        handler.forward_send(packet);
    }
}

void FragmentationLayer::send(Packet packet)
{
    log_trace("Packet [", packet.to_string(), "] sent to fragmentation layer.");
    handler.forward_send(packet);
}

void FragmentationLayer::receive(Packet packet)
{
    log_trace("Packet [", packet.to_string(), "] received on fragmentation layer.");
    handler.forward_receive(packet);

    if (packet.data.header.get_message_type() != MessageType::APPLICATION)
        return;

    std::string message_id = get_message_identifier(packet);

    assembler_map_mutex.lock();
    if (!assembler_map.contains(message_id))
        assembler_map.emplace(message_id, FragmentAssembler());

    FragmentAssembler &assembler = assembler_map.at(message_id);
    assembler.add_packet(packet);

    if (!assembler.has_received_all_packets())
    {
        assembler_map_mutex.unlock();
        return;
    }

    assembler_map_mutex.unlock();
    log_debug("Received all fragments; notifying connection.");
    handler.notify(MessageDefragmentationIsComplete(packet));
}

void FragmentationLayer::attach(EventBus &bus)
{
    obs_forward_defragmented_message.on(std::bind(&FragmentationLayer::forward_defragmented_message, this, _1));
    bus.attach(obs_forward_defragmented_message);
}

void FragmentationLayer::forward_defragmented_message(const ForwardDefragmentedMessage &event)
{
    log_debug("pipipi");
    Packet &packet = event.packet;
    std::string message_id = get_message_identifier(packet);

    assembler_map_mutex.lock();
    Message const message = assembler_map.at(message_id).assemble();
    assembler_map.erase(message_id);
    assembler_map_mutex.unlock();

    handler.forward_receive(message);
}