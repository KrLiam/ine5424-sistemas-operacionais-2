#include "pipeline.h"

Pipeline::Pipeline(GroupRegistry *gr, Channel *channel)
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


PipelineStep* Pipeline::get_step(int step_index) {
    int total_layers = layers.size();

    if (step_index >= total_layers) {
        return nullptr;
    }
    
    return layers.at(step_index);
}

void Pipeline::send(Message message) {
    send(message, layers.size() - 1);
}
void Pipeline::send(Message message, int step_index) {
    PipelineStep* step = get_step(step_index);
    if (step) step->send(message);
}
void Pipeline::send(Packet packet, int step_index) {
    PipelineStep* step = get_step(step_index);
    if (step) step->send(packet);

}

void Pipeline::receive(Message message, int step_index) {
    PipelineStep* step = get_step(step_index);
    if (step) step->receive(message);

}
void Pipeline::receive(Packet packet, int step_index) {
    PipelineStep* step = get_step(step_index);
    if (step) step->receive(packet);

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