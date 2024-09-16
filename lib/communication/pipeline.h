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

    GroupRegistry *gr;

    friend PipelineHandler;

    PipelineStep* get_step(int step_index);

    void send(Message message, int step_index);
    void send(Packet packet, int step_index);
    
    void receive(Message message, int step_index);
    void receive(Packet packet, int step_index);
public:
    Pipeline(GroupRegistry *gr, Channel *channel);

    ~Pipeline();

    void send(Message);
    void send(Packet);

    bool can_forward_to_application();
    void forward_to_application(Message message);
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &get_incoming_buffer();

    void stop_transmission(Packet packet)
    {
        reinterpret_cast<TransmissionLayer*>(layers.at(0))->stop_transmission(packet);
    }
};