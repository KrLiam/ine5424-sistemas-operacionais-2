#pragma once

#include <random>
#include <sstream>
#include <string.h>

#include "utils/random.h"

struct UUID {
    static const int MAX_SIZE = 36;

    UUID();

    UUID(std::string uuid);

    bool operator ==(const UUID& other) const;

    std::string as_string() const;

private:
    std::string uuid;
};

template<> struct std::hash<UUID> {
    std::size_t operator()(const UUID& u) const {
        return std::hash<std::string>()(u.as_string());
    }
};

std::ostream& operator<<(std::ostream& os, const UUID& uuid);

namespace uuid
{
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::string generate_uuid_v4();
}
