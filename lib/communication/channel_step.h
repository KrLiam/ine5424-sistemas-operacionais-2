
#include "communication/pipeline_step.h"
#include "channels/channel.h"

class ChannelStep : public PipelineStep
{
    Channel& channel;
public:
    ChannelStep(StepControl control, Channel& channel)
        : PipelineStep(control), channel(channel) {}

    void send(Segment &segment);

    void receive(Segment &segment);
};

void ChannelStep::send(Segment &segment) {
    channel.send(segment);
}

void ChannelStep::receive(Segment &segment) {
    control.forward_receive(segment);
}