#pragma once

#include <mutex>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/date.h"

struct FaultConfig {
    std::vector<int> faults;
};

class FaultInjectionLayer : public PipelineStep {
    std::vector<int> fault_queue;
    std::mutex mutex_fault_queue;
    
    int min_delay;
    int max_delay;
    double lose_chance;
public:
    FaultInjectionLayer(PipelineHandler handler);
    FaultInjectionLayer(PipelineHandler handler, int min_delay, int max_delay, double lose_chance);

    void enqueue_fault(int delay);
    void enqueue_fault(const std::vector<int>& faults);

    void receive(Packet packet);

    void proceed_receive(Packet packet);
};