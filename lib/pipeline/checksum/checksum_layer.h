#include <thread>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/date.h"

class ChecksumLayer : public PipelineStep {
public:
    ChecksumLayer(PipelineHandler handler);

    ~ChecksumLayer();

    void send(Packet packet);
    
    void receive(Packet packet);
};