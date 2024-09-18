#include "pipeline/pipeline.h"
#include "pipeline/channel/channel_layer.h"
#include "pipeline/fault_injection/fault_injection_layer.h"


Pipeline::Pipeline(GroupRegistry *gr, Channel *channel) : gr(gr)
{
    PipelineHandler handler = PipelineHandler(*this, -1);

    layers.push_back(new ChannelLayer(handler.at_index(0), *channel));
    layers.push_back(new FaultInjectionLayer(handler.at_index(1)));
    layers.push_back(new TransmissionLayer(handler.at_index(2), gr, channel));
    layers.push_back(new FragmentationLayer(handler.at_index(3), gr));
}

Pipeline::~Pipeline()
{
    for (auto layer : layers)
        delete layer;
}

PipelineStep *Pipeline::get_step(int step_index)
{
    int total_layers = layers.size();

    if (step_index >= total_layers)
    {
        return nullptr;
    }

    return layers.at(step_index);
}

void Pipeline::send(Message message)
{
    send(message, layers.size() - 1);
}
void Pipeline::send(Packet packet)
{
    send(packet, layers.size() - 1);
}
void Pipeline::send(Message message, int step_index)
{
    PipelineStep *step = get_step(step_index);
    if (step)
        step->send(message);
}
void Pipeline::send(Packet packet, int step_index)
{
    PipelineStep *step = get_step(step_index);
    if (step)
        step->send(packet);
}

void Pipeline::receive(Message message, int step_index)
{
    PipelineStep *step = get_step(step_index);
    if (step)
    {
        step->receive(message);
        return;
    }
    // TODO: na nova tentativa de conexão, dá pra chamar um metodo q faz a msm coisa q o establish_connections(), só q pra uma só
    Connection &conn = gr->get_connection(message.origin);
    conn.receive(message);
}
void Pipeline::receive(Packet packet, int step_index)
{
    PipelineStep *step = get_step(step_index);
    if (step)
    {
        step->receive(packet);
        return;
    }
    // TODO: na nova tentativa de conexão, dá pra chamar um metodo q faz a msm coisa q o establish_connections(), só q pra uma só
    Connection &conn = gr->get_connection(packet.meta.origin);
    conn.receive(packet);
}
