#pragma once

#include <vector>

#include "channels/channel.h"
#include "communication/pipeline_step.h"
#include "communication/reliable_communication.h"

class PipelineStep;
class ReliableCommunication;
class TransmissionLayer;
class FragmentationLayer;
class ProcessLayer;

class Pipeline
{
    std::vector<PipelineStep> layers;

public:
    Pipeline(ReliableCommunication *comm, Channel *channel);

    void send_to(unsigned int layer, char *m);

    void receive_on(unsigned int layer, char *m);
};