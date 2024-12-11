#include "pipeline/transmission/transmission_layer.h"

using namespace std::placeholders;

bool TransmissionKey::operator==(const TransmissionKey& other) const {
    return id == other.id
        && destination == other.destination;
}

TransmissionLayer::TransmissionLayer(PipelineHandler handler, NodeMap &nodes, Timer& timer)
    : PipelineStep(handler), nodes(nodes), timer(timer)
{
}

TransmissionLayer::~TransmissionLayer()
{
}

TransmissionKey TransmissionLayer::create_key(const MessageIdentity& id, const SocketAddress& remote) {
    return {
        id,
        message_type::is_broadcast(id.msg_type) ? SocketAddress{BROADCAST_ADDRESS, 0} : remote
    };
}

bool TransmissionLayer::has_queue(const TransmissionKey& key) {
    queue_mutex.lock();
    bool result = queue_map.contains(key);
    queue_mutex.unlock();
    return result;
}
TransmissionQueue& TransmissionLayer::get_queue(const TransmissionKey& key) {
    queue_mutex.lock();
    if (!has_queue(key)) {
        auto queue = std::make_shared<TransmissionQueue>(handler, nodes, timer);

        queue_map.insert({key, queue});
    }
    queue_mutex.unlock();

    return *queue_map.at(key);
}
void TransmissionLayer::clear_queue(const TransmissionKey& key) {
    queue_mutex.lock();
    queue_map.erase(key);
    queue_mutex.unlock();
}

void TransmissionLayer::attach(EventBus& bus) {
    obs_ack_received.on(std::bind(&TransmissionLayer::ack_received, this, _1));
    bus.attach(obs_ack_received);
    obs_pipeline_cleanup.on(std::bind(&TransmissionLayer::pipeline_cleanup, this, _1));
    bus.attach(obs_pipeline_cleanup);
    obs_node_death.on(std::bind(&TransmissionLayer::node_death, this, _1));
    bus.attach(obs_node_death);
}

void TransmissionLayer::send(Packet packet)
{
    const PacketMetadata& meta = packet.meta;
    const PacketHeader& header = packet.data.header;

    if (!packet.silent()) {
        log_trace("Packet [", packet.to_string(PacketFormat::SENT), "] sent to transmission layer.");
    }

    if (!packet.meta.expects_ack)
    {
        if (!packet.silent()) {
            log_debug("Packet [", packet.to_string(PacketFormat::SENT), "] does not require ACK, sending forward.");
        }
        handler.forward_send(packet);
        return;
    }

    TransmissionKey key = create_key(header.id, meta.destination);

    queue_mutex.lock();
    TransmissionQueue& queue = get_queue(key);
    queue.add_packet(packet);
    queue_mutex.unlock();
}

void TransmissionLayer::ack_received(const PacketAckReceived& event) {    
    Packet& packet = event.ack_packet;

    const MessageIdentity& id = packet.data.header.id;
    TransmissionKey key = create_key(id, packet.meta.origin);

    queue_mutex.lock();
    if (has_queue(key)) {
        TransmissionQueue& queue = get_queue(key);
        queue.receive_ack(packet);
    }
    queue_mutex.unlock();
}

void TransmissionLayer::pipeline_cleanup(const PipelineCleanup& event) {    
    clear_queue(create_key(event.id, event.destination));
}

void TransmissionLayer::node_death(const NodeDeath& event) {
    queue_mutex.lock();
    for (auto& [_, queue] : queue_map) {
        queue->discard_node(event.remote_node);
    }
    queue_mutex.unlock();
}

void TransmissionLayer::receive(Packet packet)
{
    if (!packet.silent()) {
        log_trace("Packet [", packet.to_string(PacketFormat::RECEIVED), "] received on transmission layer.");
    }

    /*if (!nodes.contains(packet.meta.origin))
    {
        log_debug("Packet ", packet.to_string(PacketFormat::RECEIVED), " does not originate from group; dropping.");
        return;
    }*/

    handler.forward_receive(packet);
}
