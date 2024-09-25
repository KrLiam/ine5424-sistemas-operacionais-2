#include <iostream>
#include "core/packet.h"

class CRC16
{
public:
    static const unsigned short POLYNOMIAL = 0x1021;
    static const unsigned short INITIAL_VALUE = 0xFFFF;

    static unsigned short calculate(const char *data, size_t length);
};