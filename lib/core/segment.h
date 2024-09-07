#pragma once

#include "utils/config.h"
#include "constants.h"
#include <cstring>
#include <cstdint>

struct PacketHeader
{
    int id : 32;
    int fragment_num : 32;
    int checksum : 16;
    int window : 16;
    int type : 4;
    int more_fragments : 1;
    int ack : 1;
    int reserved : 10;
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
