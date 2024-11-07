#pragma once

#include <mutex>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/config.h"
#include "utils/date.h"


class FaultInjectionLayer : public PipelineStep {
    std::vector<int> fault_queue;
    std::mutex mutex_fault_queue;
    
   FaultConfig config;
public:
    FaultInjectionLayer(PipelineHandler handler);
    FaultInjectionLayer(PipelineHandler handler, FaultConfig config);

    void enqueue_fault(int delay);
    void enqueue_fault(const std::vector<int>& faults);

    void receive(Packet packet);

    void proceed_receive(Packet packet);
};