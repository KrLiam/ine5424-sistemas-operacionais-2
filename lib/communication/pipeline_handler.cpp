#include "pipeline_handler.h"
#include "pipeline.h"

PipelineHandler::PipelineHandler(Pipeline& pipeline, int step_index)
    : pipeline(pipeline), step_index(step_index)
{
}

void PipelineHandler::forward_send(char *m)
{
    pipeline.send_to(step_index - 1, m);
}

void PipelineHandler::forward_receive(char *m)
{
    pipeline.receive_on(step_index + 1, m);
}

bool PipelineHandler::can_forward_to_application()
{
    return pipeline.can_forward_to_application();
}

void PipelineHandler::forward_to_application(Message message)
{
    pipeline.forward_to_application(message);
}