#pragma once

#include "core/message.h"

struct PacketHeader
{
    unsigned int msg_num : 32;
    unsigned int fragment_num : 32;
    unsigned int checksum : 16;
    unsigned int ack : 1;
    unsigned int rst : 1;
    unsigned int syn : 1;
    unsigned int fin : 1;
    unsigned int end : 1;
    unsigned int type : 4;

    uint32_t get_message_number() const
    {
        return (uint32_t)msg_num;
    }

    uint32_t get_fragment_number() const
    {
        return (uint32_t)fragment_num;
    }

    bool is_ack() const
    {
        return (bool)ack;
    }

    bool is_end() const
    {
        return (bool)end;
    }

    bool is_syn() const
    {
        return (bool)syn;
    }

    bool is_rst() const
    {
        return (bool)rst;
    }

    bool is_fin() const
    {
        return (bool)fin;
    }

    MessageType get_message_type() const
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
    UUID transmission_uuid{""};
    SocketAddress origin = {{0, 0, 0, 0}, 0};
    SocketAddress destination = {{0, 0, 0, 0}, 0};
    int message_length = 0;
    bool expects_ack = 0;
};


enum PacketFormat {
    ALL = 0,
    SENT = 1,
    RECEIVED = 2
};

struct Packet
{
    PacketData data;
    PacketMetadata meta;


    std::string to_string(PacketFormat type = PacketFormat::ALL) const
    {
        const PacketHeader& header = data.header;

        std::string flags;
        if (header.type == MessageType::APPLICATION) flags += "DATA";
        if (header.is_syn()) flags += flags.length() ? "+SYN" : "SYN";
        if (header.is_rst()) flags += flags.length() ? "+RST" : "RST";
        if (header.is_fin()) flags += flags.length() ? "+FIN" : "FIN";
        if (header.is_ack()) flags += flags.length() ? "+ACK" : "ACK";
        if (header.is_end()) flags += flags.length() ? "+END" : "SYN";

        std::string origin = meta.origin.to_string();
        std::string destination = meta.destination.to_string();

        if (type == PacketFormat::RECEIVED) {
            return format(
                "%s %u/%u from %s",
                flags.c_str(),
                data.header.msg_num,
                data.header.fragment_num,
                origin.c_str()
            );
        }
        
        if (type == PacketFormat::SENT) {
            return format(
                "%s %u/%u to %s",
                flags.c_str(),
                data.header.msg_num,
                data.header.fragment_num,
                destination.c_str()
            );
        }

        return format(
            "%s %u/%u from %s to %s",
            flags.c_str(),
            data.header.msg_num,
            data.header.fragment_num,
            origin.c_str(),
            destination.c_str()
        );
    }

    bool operator==(const Packet &other) const
    {
        return meta.origin == other.meta.origin && meta.destination == other.meta.destination && meta.message_length == other.meta.message_length && data.header.msg_num == other.data.header.msg_num && data.header.fragment_num == other.data.header.fragment_num;
    }
};


