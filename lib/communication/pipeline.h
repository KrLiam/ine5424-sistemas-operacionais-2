#pragma once

#include <vector>

#include "channels/channel.h"
#include "communication/pipeline_handler.h"
#include "communication/pipeline_step.h"
#include "communication/reliable_communication.h"
#include "communication/transmission_layer.h"
#include "communication/fragmentation_layer.h"
#include "communication/process_layer.h"

class PipelineStep;
class ReliableCommunication;

class Pipeline
{
    std::vector<PipelineStep*> layers;

public:
    Pipeline(ReliableCommunication *comm, Channel *channel);

    void send_to(unsigned int layer, char *m);

    void receive_on(unsigned int layer, char *m);
};