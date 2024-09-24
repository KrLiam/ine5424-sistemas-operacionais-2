#pragma once

#include <vector>

#include "pipeline/pipeline_handler.h"
#include "pipeline/pipeline_step.h"
#include "communication/group_registry.h"
#include "pipeline/transmission/transmission_layer.h"
#include "pipeline/fragmentation/fragmentation_layer.h"
#include "pipeline/fault_injection/fault_injection_layer.h"
#include "core/event_bus.h"

class PipelineStep;
class ReliableCommunication;

class Pipeline
{
private:
    std::vector<PipelineStep *> layers;
    EventBus event_bus;

    GroupRegistry *gr;

    friend PipelineHandler;

    PipelineStep *get_step(int step_index);

    void attach_layers();

    void send(Message message, int step_index);
    void send(Packet packet, int step_index);

    void receive(Message message, int step_index);
    void receive(Packet packet, int step_index);

    TransmissionLayer *get_transmission_layer()
    {
        return reinterpret_cast<TransmissionLayer *>(layers.at(TRANSMISSION_LAYER));
    }
    FragmentationLayer *get_fragmentation_layer()
    {
        return reinterpret_cast<FragmentationLayer *>(layers.at(FRAGMENTATION_LAYER));
    }

public:
    static const unsigned int CHANNEL_LAYER = 0;
    static const unsigned int FAULT_INJECTION_LAYER = 1;
    static const unsigned int CHECKSUM_LAYER = 2;
    static const unsigned int TRANSMISSION_LAYER = 3;
    static const unsigned int FRAGMENTATION_LAYER = 4;

    Pipeline(GroupRegistry *gr, const FaultConfig& fault_config);

    ~Pipeline();

    template <typename T>
    void attach(Observer<T>& observer) {
        event_bus.attach(observer);
    };

    template <typename T>
    void notify(const T& event) {
        event_bus.notify(event);
    };

    void send(Message);
    void send(Packet);
};