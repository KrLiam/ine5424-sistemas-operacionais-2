#include "core/segment.h"
#include "core/constants.h"
#include "core/buffer.h"

class PipelineStep;

class StepControl
{
    const std::vector<PipelineStep *> *layers;
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Segment> *send_buffer;
    int current = 0;

public:
    StepControl(
        std::vector<PipelineStep *> *layers,
        Buffer<INTERMEDIARY_BUFFER_ITEMS, Segment> *send_buffer,
        int current);

    bool resend(Segment &segment);

    void forward_send(Segment &segment);

    void forward_receive(Segment &segment);
};

class PipelineStep
{
protected:
    StepControl control;

public:
    PipelineStep(StepControl control) : control(control) {}

    virtual void send(Segment &segment);

    virtual void receive(Segment &segment);
};
