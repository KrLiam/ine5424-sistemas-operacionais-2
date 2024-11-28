#pragma once

#include <random>
#include <sstream>
#include <string.h>
#include <unordered_set>

#include "utils/random.h"

struct UUID {
    static const int MAX_SIZE = 36;
    static const int SERIALIZED_SIZE = 32;

    UUID();

    UUID(std::string uuid);

    bool operator ==(const UUID& other) const;

    std::string as_string() const;

    std::string serialize() const {
        std::string serialized = as_string();
        std::erase(serialized, '-');
        return serialized;
    }
    static UUID deserialize(const char *serialized)
    {
        std::string deserialized;

        int pos = 0;
        std::unordered_set<int> dash_positions = {8, 13, 18, 23};
        for (int i = 0; i < UUID::MAX_SIZE; i++) {
            if (dash_positions.contains(i)) {
                deserialized += '-';
                continue;
            }
            deserialized += serialized[pos];
            pos++;
        }

        return UUID(deserialized);
    }

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
