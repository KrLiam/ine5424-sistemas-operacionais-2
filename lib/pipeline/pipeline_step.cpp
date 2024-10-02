#include "pipeline/pipeline_step.h"

PipelineStep::PipelineStep(PipelineHandler &handler) : handler(handler)
{
}

PipelineStep::~PipelineStep()
{
}

void PipelineStep::attach(EventBus&) {}

void PipelineStep::send(Message message)
{
    handler.forward_send(message);
}
void PipelineStep::send(Packet packet)
{
    handler.forward_send(packet);
}

void PipelineStep::receive(Message message)
{
    handler.forward_receive(message);
}
void PipelineStep::receive(Packet packet)
{
    handler.forward_receive(packet);
}
