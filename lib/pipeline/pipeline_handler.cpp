#include "pipeline/pipeline_handler.h"
#include "pipeline/pipeline.h"

PipelineHandler::PipelineHandler(Pipeline& pipeline, EventBus& event_bus, int step_index)
    : pipeline(pipeline), event_bus(event_bus), step_index(step_index)
{
}

void PipelineHandler::forward_send(Packet packet)
{
    pipeline.send(packet, step_index - 1);
}
void PipelineHandler::forward_send(Message message)
{
    pipeline.send(message, step_index - 1);
}

void PipelineHandler::forward_receive(Packet packet)
{
    pipeline.receive(packet, step_index + 1);
}
void PipelineHandler::forward_receive(Message message)
{
    pipeline.receive(message, step_index + 1);
}
