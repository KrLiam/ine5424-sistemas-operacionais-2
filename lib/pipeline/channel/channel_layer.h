#include <thread>

#include "channels/channel.h"
#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/date.h"
#include "utils/log.h"

class ChannelLayer : public PipelineStep {
    Channel* channel;
    std::thread receiver_thread;

    void receiver();
public:
    ChannelLayer(PipelineHandler handler, SocketAddress local_address);

    ~ChannelLayer();

    void send(Packet packet);
    
    void receive(Packet packet);
};