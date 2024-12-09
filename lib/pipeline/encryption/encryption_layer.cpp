#include "pipeline/encryption/encryption_layer.h"
#include "utils/log.h"

using namespace std::placeholders;

GroupInfo::GroupInfo(const std::string &id, const ByteArray &key)
    : id(id), key(key), hash(std::hash<ByteArray>()(key)) {}

EncryptionLayer::EncryptionLayer(PipelineHandler handler, const Config& config) : PipelineStep(handler), config(config) {}

EncryptionLayer::~EncryptionLayer() {}

const std::unordered_map<uint64_t, GroupInfo>& EncryptionLayer::get_groups() const { return groups; }

void EncryptionLayer::attach(EventBus &bus)
{
    obs_join_group.on(std::bind(&EncryptionLayer::join_group, this, _1));
    obs_leave_group.on(std::bind(&EncryptionLayer::leave_group, this, _1));
    bus.attach(obs_join_group);
    bus.attach(obs_leave_group);
}

void EncryptionLayer::send(Packet packet)
{
    if (!packet.silent()) {
        log_trace("Packet ", packet.to_string(PacketFormat::SENT), " sent to encryption layer.");
    }

    if (config.disable_encryption) {
        handler.forward_send(packet);
        return;
    }

    uint64_t key_hash = packet.data.header.key_hash;

    if (key_hash != 0) {
        if (!groups.contains(key_hash)) {
            log_warn("Group with hash ", key_hash, " does not exist. Dropping packet ", packet.to_string(PacketFormat::SENT), ".");
            return;
        }
        const GroupInfo& group = groups.at(key_hash);
        ByteArray key = group.key;

        char* start = reinterpret_cast<char*> (&packet.data.header.id);
        int encryption_size = sizeof(packet.data.header) - sizeof(packet.data.header.key_hash) + packet.meta.message_length;
        ByteArray content(start, start + encryption_size);

        ByteArray enc;
        ByteArray::size_type enc_len = Aes256::encrypt(key, content, enc);

        memcpy(&packet.data.header.id, &enc[0], enc_len);
        packet.meta.message_length = enc_len - sizeof(PacketHeader) + sizeof(key_hash);
        packet.data.header.key_hash = key_hash;
    }

    handler.forward_send(packet);
}

void EncryptionLayer::receive(Packet packet)
{
    if (!packet.silent()) {
        log_trace("Packet from ", packet.meta.origin.to_string(), " received on encryption layer.");
    }

    if (config.disable_encryption) {
        handler.forward_receive(packet);
        return;
    }

    uint64_t key_hash = packet.data.header.key_hash;

    if (key_hash != 0) {
        if (!groups.contains(key_hash)) return;
        const GroupInfo& group = groups.at(key_hash);
        ByteArray key = group.key;

        char* start = reinterpret_cast<char*> (&packet.data.header.id);
        int encryption_size = sizeof(packet.data.header) - sizeof(packet.data.header.key_hash) + packet.meta.message_length;
        ByteArray enc(start, start + encryption_size);

        ByteArray dec;
        ByteArray::size_type dec_len = Aes256::decrypt(key, enc, dec);

        memcpy(&packet.data.header.id, &dec[0], dec_len);
        packet.meta.message_length = dec_len - sizeof(PacketHeader) + sizeof(key_hash);
    }
    packet.meta.silent = packet.data.header.get_message_type() == MessageType::HEARTBEAT;

    handler.forward_receive(packet);
}

void EncryptionLayer::join_group(const JoinGroup& event)
{
    GroupInfo group(event.id, event.key);

    mtx_groups.lock();
    groups.emplace(group.hash, group);
    mtx_groups.unlock();
    log_print("Local node joined group '", group.id, "'.");
}

void EncryptionLayer::leave_group(const LeaveGroup& event)
{
    const uint64_t key_hash = event.key_hash;

    mtx_groups.lock();
    if (groups.contains(key_hash)) {
        groups.erase(key_hash);
        log_info("Local node left group '", event.id, "'.");
    } else {
        log_warn("Local node does not belong to group '", event.id, "'.");
    }
    mtx_groups.unlock();
}
