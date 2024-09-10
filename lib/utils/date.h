#pragma once

#include <chrono>

class DateUtils
{
public:
    static unsigned int now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
};
