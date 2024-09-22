#pragma once

#include <random>
#include <sstream>

struct UUID {
    UUID();
    UUID(uint64_t most, uint64_t least);

    uint64_t get_least() const;
    uint64_t get_most() const;

    bool operator ==(const UUID& other) const;

private:
    uint64_t most;
    uint64_t least;
};

template<> struct std::hash<UUID> {
    std::size_t operator()(const UUID& u) const {
        return std::hash<uint64_t>()(u.get_most()) ^ std::hash<uint64_t>()(u.get_least());
    }
};

std::ostream& operator<<(std::ostream& os, const UUID& uuid);

namespace uuid
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    static std::uniform_int_distribution<uint64_t> dis64;

    UUID generate_uuid();

    std::string generate_uuid_v4();
}
