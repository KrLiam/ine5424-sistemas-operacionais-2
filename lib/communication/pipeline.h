#pragma once

#include <vector>

#include "channels/channel.h"
#include "communication/pipeline_handler.h"
#include "communication/pipeline_step.h"
#include "communication/group_registry.h"
#include "communication/transmission_layer.h"
#include "communication/fragmentation_layer.h"
#include "communication/process_layer.h"

class PipelineStep;
class ReliableCommunication;

class Pipeline
{
    std::vector<PipelineStep *> layers;

    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> incoming_buffer{"incoming messages"};

public:
    Pipeline(GroupRegistry &gr, Channel *channel);

    ~Pipeline();

    void send_to(unsigned int layer, char *m);
    void receive_on(unsigned int layer, char *m);

    void forward_to_application(Message message);
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &get_incoming_buffer();
};