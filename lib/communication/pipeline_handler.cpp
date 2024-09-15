#include "pipeline_handler.h"
#include "pipeline.h"

PipelineHandler::PipelineHandler(Pipeline& pipeline, int step_index)
    : pipeline(pipeline), step_index(step_index)
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

bool PipelineHandler::can_forward_to_application()
{
    return pipeline.can_forward_to_application();
}

void PipelineHandler::forward_to_application(Message message)
{
    pipeline.forward_to_application(message);
}