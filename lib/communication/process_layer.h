#pragma once

#include "communication/pipeline_step.h"
#include "core/segment.h"

class ProcessLayer : public PipelineStep
{
public:
    ProcessLayer(PipelineHandler handler, GroupRegistry *gr);
    ~ProcessLayer() override;

    void service();

    void send(Message);

    void receive(Message);
};
