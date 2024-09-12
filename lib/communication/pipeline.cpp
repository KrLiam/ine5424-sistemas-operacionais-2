#include "pipeline.h"

Pipeline::Pipeline(GroupRegistry &gr, Channel *channel)
{
    PipelineHandler handler = PipelineHandler(*this);    

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