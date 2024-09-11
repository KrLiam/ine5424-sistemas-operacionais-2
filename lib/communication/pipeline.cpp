#include "pipeline.h"

Pipeline::Pipeline(ReliableCommunication *comm, Channel *channel)
{
    std::function<void(unsigned int, char *)> send_callback = std::bind(&Pipeline::send_to, this, std::placeholders::_1, std::placeholders::_2);
    std::function<void(unsigned int, char *)> receive_callback = std::bind(&Pipeline::receive_on, this, std::placeholders::_1, std::placeholders::_2);
    PipelineHandler handler = PipelineHandler(send_callback, receive_callback);

    layers.push_back(new TransmissionLayer(handler, comm, channel));
    layers.push_back(new FragmentationLayer(handler, comm));
    layers.push_back(new ProcessLayer(handler, comm));
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