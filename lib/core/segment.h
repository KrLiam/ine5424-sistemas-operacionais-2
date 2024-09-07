#pragma once

#include "utils/config.h"
#include "constants.h"
#include <cstring>
#include <cstdint>

enum MessageType {
    DATA = 0,
    HANDSHAKE = 1,
    DISCOVER = 2,
    HEARTBEAT = 3
};

struct PacketHeader
{
    int seq_num : 32;
    int fragment_num : 32;
    int checksum : 16;
    int window : 16;
    int type : 4;
    int ack: 1;
    int more_fragments : 1;
    int reserved : 11;
};

struct Packet
{
    static const int SIZE = 1280; // arrumar dps pra tamanho variavel
    static const int DATA_SIZE = SIZE - sizeof(PacketHeader);

    static Packet from(PacketHeader packet_header, const char *buffer);

    PacketHeader header;
    char data[DATA_SIZE];
};

struct Segment
{
    // campos de metadados
    SocketAddress origin;
    SocketAddress destination;
    int data_size = Packet::SIZE; // arrumar dps pra tamanho variavel

    Packet packet;
};
