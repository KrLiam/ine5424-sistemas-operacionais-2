#include "pipeline.h"

Pipeline::Pipeline(GroupRegistry &gr, Channel *channel)
{
    PipelineHandler handler = PipelineHandler(*this, -1);    

    layers.push_back(new TransmissionLayer(handler.at_index(0), gr, channel));
    layers.push_back(new FragmentationLayer(handler.at_index(1), gr));
    layers.push_back(new ProcessLayer(handler.at_index(2), gr));
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

void Pipeline::send(char *m)
{
    int top_layer_index = layers.size() - 1;
    return send_to(top_layer_index, m);
}

void Pipeline::receive_on(unsigned int layer, char *m)
{
    if (layer >= layers.size())
        return;
    layers.at(layer)->receive(m);
}

bool Pipeline::can_forward_to_application()
{
    return incoming_buffer.can_produce();
}

void Pipeline::forward_to_application(Message message)
{
    incoming_buffer.produce(message);
}

Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &Pipeline::get_incoming_buffer()
{
    return incoming_buffer;
}