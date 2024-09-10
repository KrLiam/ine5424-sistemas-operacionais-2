#pragma once

#include "communication/pipeline_step.h"
#include "core/segment.h"

class ReliableCommunication;
class Pipeline;

class ProcessLayer : public PipelineStep
{
private:
    ReliableCommunication *comm;

public:
    ProcessLayer(PipelineHandler& handler, ReliableCommunication *comm);

    void service();

    void send(char *m);

    void receive(char *m);
};
