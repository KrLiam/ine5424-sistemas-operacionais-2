#include <thread>

#include "channels/channel.h"
#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "core/node.h"
#include "utils/date.h"
#include "utils/log.h"

class ChannelLayer : public PipelineStep {
    std::unique_ptr<Channel> channel;
    std::thread receiver_thread;
    NodeMap& nodes;

    void receiver();
public:
    ChannelLayer(PipelineHandler handler, SocketAddress local_address, NodeMap& nodes, EventBus& event_bus);

    ~ChannelLayer();

    void send(Packet packet);
    
    void receive(Packet packet);
};