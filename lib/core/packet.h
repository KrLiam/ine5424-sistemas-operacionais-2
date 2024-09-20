#pragma once

#include "core/message.h"

struct PacketHeader
{
    unsigned int msg_num : 32;
    unsigned int fragment_num : 32;
    unsigned int checksum : 16;
    unsigned int window : 16;
    unsigned int ack : 1;
    unsigned int rst : 1;
    unsigned int syn : 1;
    unsigned int fin : 1;
    unsigned int extra : 4;
    unsigned int end : 1;
    unsigned int type : 4;
    unsigned int reserved : 4;

    uint32_t get_message_number()
    {
        return (uint32_t)msg_num;
    }

    uint32_t get_fragment_number()
    {
        return (uint32_t)fragment_num;
    }

    bool is_ack()
    {
        return (bool)ack;
    }

    bool is_end()
    {
        return (bool)end;
    }

    bool is_syn()
    {
        return (bool)syn;
    }

    bool is_rst()
    {
        return (bool)rst;
    }

    bool is_fin()
    {
        return (bool)fin;
    }

    MessageType get_message_type()
    {
        return static_cast<MessageType>(type);
    }
};

struct PacketData
{
    static const int MAX_PACKET_SIZE = 1280;
    static const int MAX_MESSAGE_SIZE = MAX_PACKET_SIZE - sizeof(PacketHeader);

    PacketHeader header;
    char message_data[MAX_MESSAGE_SIZE];
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

    std::string to_string() const
    {
        return format("%s --> %s | %s/%s", meta.origin.to_string().c_str(), meta.destination.to_string().c_str(), std::to_string((uint32_t)data.header.msg_num).c_str(), std::to_string((uint32_t)data.header.fragment_num).c_str());
    }

    bool operator==(const Packet &other) const
    {
        return meta.origin == other.meta.origin && meta.destination == other.meta.destination && meta.message_length == other.meta.message_length && data.header.msg_num == other.data.header.msg_num && data.header.fragment_num == other.data.header.fragment_num;
    }
};
