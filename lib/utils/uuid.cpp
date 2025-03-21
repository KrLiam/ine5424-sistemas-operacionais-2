#include "utils/uuid.h"

UUID::UUID() : uuid(uuid::generate_uuid_v4()) {}

UUID::UUID(std::string uuid) : uuid(uuid) {}

bool UUID::operator==(const UUID& other) const {
    return uuid.compare(other.as_string()) == 0;
}

std::string UUID::as_string() const {
    return uuid;
}

std::ostream& operator<<(std::ostream& os, const UUID& uuid) {
    return os << uuid.as_string();
}

namespace uuid {

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