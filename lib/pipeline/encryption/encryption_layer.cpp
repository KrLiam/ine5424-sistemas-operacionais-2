#include "pipeline/encryption/encryption_layer.h"
#include "utils/log.h"

EncryptionLayer::EncryptionLayer(PipelineHandler handler) : PipelineStep(handler)
{
    unsigned char test_key[32] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };

    ByteArray key(test_key_2, test_key_2 + sizeof(test_key_2));
    keys.emplace(1, key);
}

EncryptionLayer::~EncryptionLayer() {}

void EncryptionLayer::send(Packet packet)
{
    if (!packet.silent()) {
        log_trace("Packet ", packet.to_string(PacketFormat::SENT), " sent to encryption layer.");
    }

    uint64_t key_hash = 1;

    if (key_hash != 0) {
        ByteArray key = keys.at(key_hash);

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

    uint64_t key_hash = packet.data.header.key_hash;

    if (key_hash != 0) {
        if (!keys.contains(key_hash)) return;
        ByteArray key = keys.at(key_hash);

        char* start = reinterpret_cast<char*> (&packet.data.header.id);
        int encryption_size = sizeof(packet.data.header) - sizeof(packet.data.header.key_hash) + packet.meta.message_length;
        ByteArray enc(start, start + encryption_size);

        ByteArray dec;
        ByteArray::size_type dec_len = Aes256::decrypt(key, enc, dec);

        memcpy(&packet.data.header.id, &dec[0], dec_len);
        packet.meta.message_length = dec_len - sizeof(PacketHeader) + sizeof(key_hash);
        packet.meta.silent = packet.data.header.get_message_type() == MessageType::HEARTBEAT;
    }

    handler.forward_receive(packet);
}

