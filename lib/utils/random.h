#pragma once

#include <random>

namespace rc_random
{
    static std::mt19937_64 gen(time(nullptr));
    static std::uniform_int_distribution<> dis(0, RAND_MAX);
    static std::uniform_int_distribution<> dis32(0, UINT32_MAX);
    static std::uniform_int_distribution<> dis8(0, 255);
}
