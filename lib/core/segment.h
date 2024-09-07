#pragma once

#include "utils/config.h"
#include "constants.h"
#include <cstring>

struct PacketHeader
{
    char id[32];
    char fragment_num[32];
    char checksum[16];
    char window[16];
    char type[4];
    bool more_fragments;
    bool ack;
    char reserved[10];
};

struct Packet
{
    static const int SIZE = 1280; // arrumar dps pra tamanho variavel
    static const int DATA_SIZE = SIZE - sizeof(PacketHeader);

    static Packet from(const char *buffer);

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
