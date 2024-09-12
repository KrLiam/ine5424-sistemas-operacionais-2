#include "pipeline_handler.h"
#include "pipeline.h"

PipelineHandler::PipelineHandler(Pipeline& pipeline) : pipeline(pipeline)
{
}

void PipelineHandler::send_to(unsigned int layer_number, char *m)
{
    pipeline.send_to(layer_number, m);
}

void PipelineHandler::receive_on(unsigned int layer_number, char *m)
{
    pipeline.receive_on(layer_number, m);
}

void PipelineHandler::forward_to_application(Message message)
{
    pipeline.forward_to_application(message);
}