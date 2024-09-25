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
};

struct Message
{
    inline const static int MAX_SIZE = 65536;

    UUID transmission_uuid;
    uint32_t number;
    SocketAddress origin;
    SocketAddress destination;
    MessageType type;

    char data[MAX_SIZE];
    std::size_t length;

    std::string to_string() const
    {
        return format("%s -> %s", origin.to_string().c_str(), destination.to_string().c_str());
    }

};
