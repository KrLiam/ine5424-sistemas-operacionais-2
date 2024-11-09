#include <thread>

#include "pipeline/channel/channel_layer.h"
#include "utils/log.h"

ChannelLayer::ChannelLayer(PipelineHandler handler, SocketAddress local_address, const NodeMap& nodes)
    : PipelineStep(handler), nodes(nodes)
{
    channel = std::make_unique<Channel>(local_address);
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
    log_info("Initialized receiver thread.");
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
    log_info("Closed receiver thread.");
}

void ChannelLayer::send(Packet packet)
{
    const SocketAddress& destination = packet.meta.destination;

    if (destination == BROADCAST_ADDRESS) {
        for (const auto& [id, node] : nodes) {

            packet.meta.destination = node.get_address();
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