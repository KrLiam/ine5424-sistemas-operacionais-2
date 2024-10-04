#include "pipeline/transmission/transmission_layer.h"

TransmissionLayer::TransmissionLayer(PipelineHandler handler, const NodeMap &nodes)
    : PipelineStep(handler), nodes(nodes)
{
}

TransmissionLayer::~TransmissionLayer()
{
}
TransmissionQueue& TransmissionLayer::get_queue(const MessageIdentity& id) {
    if (!queue_map.contains(id)) {
        auto queue = std::make_shared<TransmissionQueue>(timer, handler, nodes);

        if (id.origin == BROADCAST_ADDRESS) {
            for (const auto& [_, node] : nodes) {
                MessageIdentity expanded_id = {
                    origin : node.get_address(),
                    msg_num : id.msg_num,
                    sequence_type : id.sequence_type
                };
                queue_map.insert({expanded_id, queue});
                log_info("Added queue to ", expanded_id.origin.to_string(), " ", expanded_id.msg_num, " ", expanded_id.sequence_type);
            }
        }
        else {
            queue_map.insert({id, queue});
            log_info("Added queue to ", id.origin.to_string(), " ", id.msg_num, " ", id.sequence_type);
        }
    }

    return *queue_map.at(id);
}
void TransmissionLayer::clear_queue(const MessageIdentity& id) {
    if (id.origin == BROADCAST_ADDRESS) {
        for (const auto& [_, node] : nodes) {
            MessageIdentity expanded_id = {
                origin : node.get_address(),
                msg_num : id.msg_num,
                sequence_type : id.sequence_type
            };
            queue_map.erase(expanded_id);
            log_info("Cleared queue to ", expanded_id.origin.to_string(), " ", expanded_id.msg_num, " ", expanded_id.sequence_type);
        }

        return;
    }
    
    queue_map.erase(id);
    log_info("Cleared queue to ", id.origin.to_string(), " ", id.msg_num, " ", id.sequence_type);

}

void TransmissionLayer::attach(EventBus& bus) {
    obs_ack_received.on(std::bind(&TransmissionLayer::ack_received, this, _1));
    bus.attach(obs_ack_received);
    obs_pipeline_cleanup.on(std::bind(&TransmissionLayer::pipeline_cleanup, this, _1));
    bus.attach(obs_pipeline_cleanup);
}

void TransmissionLayer::send(Packet packet)
{
    const PacketMetadata& meta = packet.meta;
    const PacketHeader& header = packet.data.header;

    log_trace("Packet [", packet.to_string(PacketFormat::SENT), "] sent to transmission layer.");

    if (!packet.meta.expects_ack)
    {
        log_debug("Packet [", packet.to_string(PacketFormat::SENT), "] does not require ACK, sending forward.");
        handler.forward_send(packet);
        return;
    }

    TransmissionQueue& queue = get_queue({meta.destination, header.id.msg_num, header.id.sequence_type});
    queue.add_packet(packet);
}

void TransmissionLayer::ack_received(const PacketAckReceived& event) {    
    Packet& packet = event.ack_packet;

    TransmissionQueue& queue = get_queue(packet.data.header.id);
    queue.receive_ack(packet);
}

void TransmissionLayer::pipeline_cleanup(const PipelineCleanup& event) {    
    Message& message = event.message;

    clear_queue({message.destination, message.id.msg_num, message.id.sequence_type});
}

void TransmissionLayer::receive(Packet packet)
{
    log_trace("Packet [", packet.to_string(PacketFormat::RECEIVED), "] received on transmission layer.");

    if (!nodes.contains(packet.data.header.id.origin))
    {
        log_debug("Packet ", packet.to_string(PacketFormat::RECEIVED), " does not originate from group; dropping.");
        return;
    }

    handler.forward_receive(packet);
}
