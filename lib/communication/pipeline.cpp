#include "pipeline.h"

Pipeline::Pipeline(GroupRegistry &gr, Channel *channel)
{
    std::function<void(unsigned int, char*)> send_callback = std::bind(&Pipeline::send_to, this, std::placeholders::_1, std::placeholders::_2);
    std::function<void(unsigned int, char*)> receive_callback = std::bind(&Pipeline::receive_on, this, std::placeholders::_1, std::placeholders::_2);
    std::function<void(Message)> application_forward_callback = std::bind(&Pipeline::forward_to_application, this, std::placeholders::_1);
    PipelineHandler handler = PipelineHandler(send_callback, receive_callback, application_forward_callback);    

    layers.push_back(new TransmissionLayer(handler, gr, channel));
    layers.push_back(new FragmentationLayer(handler, gr));
    layers.push_back(new ProcessLayer(handler, gr));
}

Pipeline::~Pipeline()
{
    for (auto layer : layers)
        delete layer;
}

void Pipeline::send_to(unsigned int layer, char *m)
{
    if (layer >= layers.size())
        return;
    layers.at(layer)->send(m);
}

void Pipeline::receive_on(unsigned int layer, char *m)
{
    if (layer >= layers.size())
        return;
    layers.at(layer)->receive(m);
}

void Pipeline::forward_to_application(Message message)
{
    incoming_buffer.produce(message);
}

Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &Pipeline::get_incoming_buffer()
{
    return incoming_buffer;
}