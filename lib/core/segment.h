#pragma once

#include "utils/config.h"
#include "constants.h"
#include <cstring>
#include <cstdint>

struct PacketHeader
{
    int msg_num : 32;
    int fragment_num : 32;
    int checksum : 16;
    int window : 16;
    int type : 4;
    int ack: 1;
    int more_fragments : 1;
    int reserved : 11;
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
    int time;
    int message_length;
};

struct Packet
{
    PacketData data;
    PacketMetadata meta;
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

/*
    static Message from(SocketAddress origin, SocketAddress destination, const char* data, int length)
    {
        Message message = {
            origin: origin,
            destination: destination,
            length: length
        };

        /*
        int required_packets = length / Packet::MAX_DATA_SIZE;
        Packet packets[required_packets];

        for (int i = 0; i < required_packets; i++)
        {
            packets[i].header = {
                fragment_num: i,
                type: MessageType::DATA,
                ack: 0,
                more_fragments: i != required_packets - 1
                };
            packets[i].length = std::min(length - i*Packet::MAX_DATA_SIZE, Packet::MAX_DATA_SIZE);
            strncpy(packets[i].data, &data[i*Packet::MAX_DATA_SIZE], packets[i].length);
        }

        return message;
    }*/
};
