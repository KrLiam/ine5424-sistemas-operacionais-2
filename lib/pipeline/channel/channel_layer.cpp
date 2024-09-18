#include <thread>

#include "pipeline/channel/channel_layer.h"
#include "utils/log.h"


ChannelLayer::ChannelLayer(PipelineHandler handler, Channel& channel) : PipelineStep(handler, nullptr), channel(channel) {
    receiver_thread = std::thread([this]() { receiver(); });
}

ChannelLayer::~ChannelLayer() {
    stop_thread = true;
    receiver_thread.join();
}

void ChannelLayer::receiver()
{
    while (true) {
        Packet packet = channel.receive();
        if (packet != Packet{})
        {
            receive(packet);
        }
    }
}

void ChannelLayer::send(Packet packet) {
    channel.send(packet);
}

void ChannelLayer::receive(Packet packet) {
    handler.forward_receive(packet);
}