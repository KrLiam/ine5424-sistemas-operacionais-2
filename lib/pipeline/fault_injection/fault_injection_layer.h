#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/date.h"

class FaultInjectionLayer : public PipelineStep {
    Timer timer;
    
    int min_delay;
    int max_delay;
    double lose_chance;
public:
    FaultInjectionLayer(PipelineHandler handler);
    FaultInjectionLayer(PipelineHandler handler, int min_delay, int max_delay, double lose_chance);

    void receive(Packet packet);

    void proceed_receive(Packet packet);
};