#include "pipeline/pipeline.h"
#include "pipeline/channel/channel_layer.h"
#include "pipeline/fault_injection/fault_injection_layer.h"
#include "pipeline/checksum/checksum_layer.h"

Pipeline::Pipeline(GroupRegistry *gr, const FaultConfig& fault_config) : gr(gr)
{
    PipelineHandler handler = PipelineHandler(*this, event_bus, -1);

    FaultInjectionLayer* fault_layer = new FaultInjectionLayer(handler.at_index(FAULT_INJECTION_LAYER), 200, 500, 0);
    fault_layer->enqueue_fault(fault_config.faults);

    layers.push_back(new ChannelLayer(handler.at_index(CHANNEL_LAYER), gr->get_local_node().get_address(), gr->get_nodes()));
    layers.push_back(fault_layer);
    layers.push_back(new TransmissionLayer(handler.at_index(TRANSMISSION_LAYER), gr->get_nodes()));
    layers.push_back(new ChecksumLayer(handler.at_index(CHECKSUM_LAYER)));
    layers.push_back(new FragmentationLayer(handler.at_index(FRAGMENTATION_LAYER)));

    attach_layers();
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

void Pipeline::attach_layers() {
    event_bus.clear();

    for (PipelineStep* layer : layers) {
        layer->attach(event_bus);
    }
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
