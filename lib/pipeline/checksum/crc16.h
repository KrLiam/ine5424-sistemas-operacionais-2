#include <iostream>
#include "core/packet.h"

class CRC16
{
public:
    static const uint16_t POLYNOMIAL = 0x1021;
    static const uint16_t INITIAL_VALUE = 0xFFFF;

    static uint16_t calculate(const uint8_t *data, size_t length);
};