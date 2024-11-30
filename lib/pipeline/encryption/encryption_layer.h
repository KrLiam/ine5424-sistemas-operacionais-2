#include <unordered_map>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/aes256.h"

class EncryptionLayer : public PipelineStep
{
    std::unordered_map<uint64_t, ByteArray> keys;

    void encrypt(Packet& packet);
    bool decrypt(Packet& packet);

public:
    EncryptionLayer(PipelineHandler handler);

    ~EncryptionLayer();

    void send(Packet packet);

    void receive(Packet packet);
};
