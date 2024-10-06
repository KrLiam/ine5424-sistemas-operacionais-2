#pragma once

#include "utils/config.h"
#include "utils/format.h"
#include "utils/uuid.h"
#include "constants.h"
#include <cstring>
#include <cstdint>

enum MessageType
{
    APPLICATION = 0,
    CONTROL = 1,
    HEARTBEAT = 2,
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
