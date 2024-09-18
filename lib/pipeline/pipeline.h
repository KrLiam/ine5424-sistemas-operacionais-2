#pragma once

#include <vector>

#include "channels/channel.h"
#include "pipeline/pipeline_handler.h"
#include "pipeline/pipeline_step.h"
#include "communication/group_registry.h"
#include "pipeline/transmission/transmission_layer.h"
#include "pipeline/fragmentation/fragmentation_layer.h"

class PipelineStep;
class ReliableCommunication;

class Pipeline
{
private:
    std::vector<PipelineStep *> layers;

    GroupRegistry *gr;

    friend PipelineHandler;

    PipelineStep *get_step(int step_index);

    void send(Message message, int step_index);
    void send(Packet packet, int step_index);

    void receive(Message message, int step_index);
    void receive(Packet packet, int step_index);

public:
    Pipeline(GroupRegistry *gr, Channel *channel);

    ~Pipeline();

    void send(Message);
    void send(Packet);

    void stop_transmission(Packet packet)
    {
        reinterpret_cast<TransmissionLayer *>(layers.at(0))->stop_transmission(packet);
    }

    bool is_message_complete(std::string node_id)
    {
        return reinterpret_cast<FragmentationLayer *>(layers.at(1))->is_message_complete(node_id);
    }
    Message assemble_message(std::string node_id)
    {
        return reinterpret_cast<FragmentationLayer *>(layers.at(1))->assemble_message(node_id);
    }
};