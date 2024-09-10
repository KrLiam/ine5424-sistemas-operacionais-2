#include "communication/pipeline_step.h"

PipelineStep::PipelineStep(unsigned int number, Pipeline *control) : number(number), control(control)
{
    service();
}

void PipelineStep::forward_send(char *m)
{
    control->send_to(number - 1, m);
}

void PipelineStep::forward_receive(char *m)
{
    control->receive_on(number + 1, m);
}