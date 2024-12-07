#include <mutex>
#include <unordered_map>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "core/event.h"
#include "utils/aes256.h"


struct GroupInfo {
    const std::string id;
    const ByteArray key;
    const uint64_t hash;

    GroupInfo(const std::string &id, const ByteArray &key);
};


class EncryptionLayer : public PipelineStep
{
    std::unordered_map<uint64_t, GroupInfo> groups;
    std::mutex mtx_groups;

    const Config& config;

    void encrypt(Packet& packet);
    bool decrypt(Packet& packet);

    Observer<JoinGroup> obs_join_group;
    void join_group(const JoinGroup& event);

    Observer<LeaveGroup> obs_leave_group;
    void leave_group(const LeaveGroup& event);

public:
    EncryptionLayer(PipelineHandler handler, const Config& config);

    ~EncryptionLayer();

    const std::unordered_map<uint64_t, GroupInfo>& get_groups() const;

    void attach(EventBus& bus);

    void send(Packet packet);

    void receive(Packet packet);
};
