#pragma once

#include "channels/channel.h"
#include "communication/pipeline.h"
#include "core/segment.h"
#include "core/constants.h"
#include "core/buffer.h"

class Pipeline;
class ReliableCommunication;

class PipelineStep
{
protected:
    unsigned int number;
    Pipeline *control;

    void forward_send(char *m);

    void forward_receive(char *m);

public:
    static const unsigned int TRANSMISSION_LAYER = 0;
    static const unsigned int FRAGMENTATION_LAYER = 1;
    static const unsigned int PROCESS_LAYER = 2;

    PipelineStep(unsigned int number, Pipeline *control);

    virtual void service() {};

    virtual void send(char *m) {};

    virtual void receive(char *m) {};
};
