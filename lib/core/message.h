#pragma once

#include "utils/config.h"
#include "utils/format.h"
#include "utils/uuid.h"
#include "constants.h"
#include <cstring>
#include <cstdint>

enum class MessageType
{
    // Padrão
    // <type0><type1><type2><type3><type4><type5><broadcast><application>
    // type: tipo da mensagem.
    // broadcast: se a mensagem é broadcast.
    // application: se a mensagem é da aplicação.

    // Mensagens da lib
    CONTROL   = 0b00000000,
    HEARTBEAT = 0b00000100,
    // Mensagens da aplicação
    SEND      = 0b00000001,
    BEB       = 0b00000011,
    URB       = 0b00000111,
    AB        = 0b00001011
};
// queria tanto que enums pudessem ter métodos :(
namespace message_type {
    bool is_application(MessageType type);
    bool is_broadcast(MessageType type);
};

enum MessageSequenceType {
    UNICAST = 0,
    BROADCAST = 1
};

struct MessageIdentity {
    SocketAddress origin;
    uint32_t msg_num;
    MessageSequenceType sequence_type;

    bool operator==(const MessageIdentity& other) const;
};

template<> struct std::hash<MessageIdentity> {
    std::size_t operator()(const MessageIdentity& id) const {
        return std::hash<SocketAddress>()(id.origin)
            ^ std::hash<uint32_t>()(id.msg_num)
            ^ std::hash<uint32_t>()(id.sequence_type);
    }
};


struct MessageData
{
    const char *ptr;
    std::size_t size = -1;

    MessageData(const char *ptr) : ptr(ptr) {}
    MessageData(const char *ptr, std::size_t size) : ptr(ptr), size(size) {}

    template <typename T>
    static MessageData from(T& data)
    {
        return MessageData(reinterpret_cast<char*>(&data), sizeof(T));
    }
};

struct Message
{
    inline const static int MAX_SIZE = 65536;

    MessageIdentity id;
    UUID transmission_uuid;
    SocketAddress destination;
    MessageType type;

    char data[MAX_SIZE];
    std::size_t length;

    std::string to_string() const
    {
        return format("%s -> %s", id.origin.to_string().c_str(), destination.to_string().c_str());
    }

};
