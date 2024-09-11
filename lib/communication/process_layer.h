#pragma once

#include "communication/pipeline_step.h"
#include "core/segment.h"

class ProcessLayer : public PipelineStep
{
public:
    ProcessLayer(PipelineHandler& handler, GroupRegistry& gr);

    void service();

    void send(char *m);

    void receive(char *m);
};
