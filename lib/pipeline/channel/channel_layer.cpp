#include <thread>

#include "pipeline/channel/channel_layer.h"
#include "utils/log.h"

ChannelLayer::ChannelLayer(PipelineHandler handler, Channel &channel) : PipelineStep(handler, nullptr), channel(channel)
{
    receiver_thread = std::thread([this]()
                                  { receiver(); });
}

ChannelLayer::~ChannelLayer()
{
    receiver_thread.join();
}

void ChannelLayer::receiver()
{
    log_info("Initialized receiver thread.");
    while (true)
    {
        try
        {
            Packet packet = channel.receive();
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
    channel.send(packet);
}

void ChannelLayer::receive(Packet packet)
{
    handler.forward_receive(packet);
}