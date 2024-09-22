
#include "utils/uuid.h"

UUID::UUID() : most(0), least(0) {}
UUID::UUID(uint64_t most, uint64_t least) : most(most), least(least) {}

uint64_t UUID::get_most() const { return most; }
uint64_t UUID::get_least() const { return least; }

bool UUID::operator==(const UUID& other) const {
    return most == other.most && least == other.least;
}

std::ostream& operator<<(std::ostream& os, const UUID& uuid) {
    return os << uuid.get_most() << ':' << uuid.get_least();
}

namespace uuid {

UUID generate_uuid() {
    return UUID(dis64(gen), dis64(gen));
}

std::string generate_uuid_v4()
{
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++)
    {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++)
    {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++)
    {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++)
    {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++)
    {
        ss << dis(gen);
    };
    return ss.str();
}

}