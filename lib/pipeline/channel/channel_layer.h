#include <thread>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/date.h"
#include "utils/log.h"

class ChannelLayer : public PipelineStep {
    Channel& channel;
    std::thread receiver_thread;
    bool stop_thread = false;

    void receiver();
public:
    ChannelLayer(PipelineHandler handler, Channel& channel);

    ~ChannelLayer();

    void send(Packet packet);
    
    void receive(Packet packet);
};