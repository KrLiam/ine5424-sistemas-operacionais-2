#include "communication/pipeline_step.h"

PipelineStep::PipelineStep(unsigned int number, PipelineHandler& handler, GroupRegistry& gr) : number(number), handler(handler), gr(gr)
{
}

void PipelineStep::forward_send(char *m)
{
    handler.send_to(number - 1, m);
}

void PipelineStep::forward_receive(char *m)
{
    handler.receive_on(number + 1, m);
}