
#include "communication/pipeline_step.h"

StepControl::StepControl(
    std::vector<PipelineStep *> *layers,
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Segment> *send_buffer,
    int current = 0) : layers(layers), send_buffer(send_buffer), current(current) {}

void StepControl::forward_send(Segment &segment)
{
    int i = current + 1;
    int size = layers->size();
    if (i == size)
        return;

    auto layer = layers->at(i);
    layer->send(segment);
}

void StepControl::forward_receive(Segment &segment)
{
    if (current == 0)
        return;

    auto layer = layers->at(current - 1);
    layer->receive(segment);
}

bool StepControl::resend(Segment &segment)
{
    return send_buffer->produce(segment, false);
}

void PipelineStep::send(Segment &segment)
{
    control.forward_send(segment);
}

void PipelineStep::receive(Segment &segment)
{
    control.forward_receive(segment);
}
