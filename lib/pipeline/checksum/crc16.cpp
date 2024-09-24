#include "crc16.h"

// Treats the input data as a polynomial and performs polynomial division by
// the previously defined polynomial. The remainder of this division is the CRC.
unsigned short CRC16::calculate(const char *data, size_t length)
{
    unsigned short crc = INITIAL_VALUE;
    unsigned short byte, bit_mask;
    bool xor_flag;

    for (size_t i = 0; i < length; i++)
    {
        bit_mask = 0x80;
        byte = data[i];

        for (int i = 0; i < 8; i++)
        {
            xor_flag = crc & 0x8000;
            crc <<= 1;

            if (byte & bit_mask)
                crc += 1;

            if (xor_flag)
                crc ^= POLYNOMIAL;

            bit_mask >>= 1;
        }
    }

    for (int i = 0; i < 16; i++)
    {
        xor_flag = crc & 0x8000;
        crc <<= 1;

        if (xor_flag)
            crc ^= POLYNOMIAL;
    }

    return crc;
}