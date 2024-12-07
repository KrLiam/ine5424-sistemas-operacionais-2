#include <thread>

#include "pipeline/channel/channel_layer.h"
#include "utils/log.h"

ChannelLayer::ChannelLayer(PipelineHandler handler, SocketAddress local_address, NodeMap& nodes, EventBus& event_bus)
    : PipelineStep(handler), nodes(nodes)
{
    channel = std::make_unique<Channel>(local_address, event_bus);
    receiver_thread = std::thread([this]()
                                  { receiver(); });
}

ChannelLayer::~ChannelLayer()
{
    channel->shutdown_socket();
    if (receiver_thread.joinable()) receiver_thread.join();
}

void ChannelLayer::receiver()
{
    log_debug("Initialized receiver thread.");
    while (true)
    {
        try
        {
            Packet packet = channel->receive();
            receive(packet);
        }
        catch (const std::runtime_error &e)
        {
            break;
        }
    }
    log_debug("Closed receiver thread.");
}

void ChannelLayer::send(Packet packet)
{
    const SocketAddress& destination = packet.meta.destination;
    uint64_t group_hash = packet.data.header.key_hash;

    if (destination == BROADCAST_ADDRESS) {
        for (const Node* node : nodes.get_group(group_hash)) {

            packet.meta.destination = node->get_address();
            channel->send(packet);
        }
    }
    else {
        channel->send(packet);
    }
}

void ChannelLayer::receive(Packet packet)
{
    handler.forward_receive(packet);
}