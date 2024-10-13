#pragma once

#include <random>

namespace rc_random
{
    static std::mt19937_64 gen(time(nullptr));
    static std::uniform_int_distribution<> dis(0, RAND_MAX);
}
