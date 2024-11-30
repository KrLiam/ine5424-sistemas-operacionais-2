#include <mutex>
#include <unordered_map>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "core/event.h"
#include "utils/aes256.h"

class EncryptionLayer : public PipelineStep
{
    std::unordered_map<uint64_t, ByteArray> keys;
    std::mutex mtx_keys;

    void encrypt(Packet& packet);
    bool decrypt(Packet& packet);

    Observer<JoinGroup> obs_join_group;
    void join_group(const JoinGroup& event);

    Observer<LeaveGroup> obs_leave_group;
    void leave_group(const LeaveGroup& event);

public:
    EncryptionLayer(PipelineHandler handler);

    ~EncryptionLayer();

    void attach(EventBus& bus);

    void send(Packet packet);

    void receive(Packet packet);
};
