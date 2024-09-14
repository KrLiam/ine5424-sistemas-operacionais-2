#pragma once

#include "utils/config.h"
#include "utils/format.h"
#include "utils/hash.h"
#include "constants.h"
#include <cstring>
#include <cstdint>

struct PacketHeader
{
    unsigned int msg_num : 32;
    unsigned int fragment_num : 32;
    unsigned int checksum : 16;
    unsigned int window : 16;
    unsigned int type : 4;
    unsigned int ack: 1;
    unsigned int more_fragments : 1;
    unsigned int reserved : 11;
};

struct PacketData
{
    static const int MAX_PACKET_SIZE = 1280;
    static const int MAX_MESSAGE_SIZE = MAX_PACKET_SIZE - sizeof(PacketHeader);

    static PacketData from(PacketHeader packet_header, const char *buffer);

    PacketHeader header;
    char message_data[MAX_MESSAGE_SIZE];
    // int message_length;
};

struct PacketMetadata
{
    SocketAddress origin;
    SocketAddress destination;
    uint64_t time;
    int message_length;
};

struct Packet
{
    PacketData data;
    PacketMetadata meta;

    static Packet from(char *m)
    {
        Packet p;
        memcpy(&p, m, sizeof(Packet));
        return p;
    }

    std::vector<char> as_bytes()
    {
        std::vector<char> p(sizeof(Packet));
        memcpy(&p[0], this, sizeof(Packet));
        return p;
    }

    std::string to_string() const
    {
        return format("%s --> %s | %s/%s", meta.origin.to_string().c_str(), meta.destination.to_string().c_str(), std::to_string((uint32_t)data.header.msg_num).c_str(), std::to_string((uint32_t)data.header.fragment_num).c_str());
    }

    bool operator==(const Packet& other)
    {
        return meta.origin == other.meta.origin && meta.destination == other.meta.destination && meta.message_length == other.meta.message_length && data.header.msg_num == other.data.header.msg_num && data.header.fragment_num == other.data.header.fragment_num;
    }
};

enum MessageType {
    DATA = 0,
    HANDSHAKE = 1,
    DISCOVER = 2,
    HEARTBEAT = 3
};

struct Message
{
    const static int MAX_MESSAGE_SIZE = 65536;

    SocketAddress origin;
    SocketAddress destination;
    MessageType type;

    char data[MAX_MESSAGE_SIZE];
    std::size_t length;

    static Message from(char *m)
    {
        Message message;
        memcpy(&message, m, sizeof(Message));
        return message;
    }

    std::vector<char> as_bytes()
    {
        std::vector<char> msg(sizeof(Message));
        memcpy(&msg[0], this, sizeof(Message));
        return msg;
    }

    std::string to_string() const
    {
        return format("%s -> %s", origin.to_string().c_str(), destination.to_string().c_str());
    }
};
