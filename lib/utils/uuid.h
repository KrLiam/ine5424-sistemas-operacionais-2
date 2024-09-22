#pragma once

#include <random>
#include <sstream>

namespace uuid
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    static std::uniform_int_distribution<> dis3(0, 65536);

    uint64_t generate_uuid();

    std::string generate_uuid_v4();
}
