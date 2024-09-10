#include "pipeline.h"

Pipeline::Pipeline(ReliableCommunication *comm, Channel *channel)
{
    //layers.push_back(new TransmissionLayer(this, comm, channel));
    //layers.push_back(new FragmentationLayer(this, comm));
    //layers.push_back(new ProcessLayer(this, comm));
}

void Pipeline::send_to(unsigned int layer, char *m)
{
    if (layer < 0 || layer >= layers.size())
        return;
    layers.at(layer).send(m);
}

void Pipeline::receive_on(unsigned int layer, char *m)
{
    if (layer < 0 || layer >= layers.size())
        return;
    layers.at(layer).receive(m);
}