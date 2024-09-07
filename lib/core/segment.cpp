#include "core/segment.h"

Packet Packet::from(const char *buffer)
{
    Packet packet;
    strncpy(packet.data, buffer, Packet::DATA_SIZE);
    return packet;
};
