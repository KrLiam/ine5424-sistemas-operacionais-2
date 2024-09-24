#include "crc16.h"

// Treats the input data as a polynomial and performs polynomial division by
// the previously defined polynomial. The remainder of this division is the CRC.
uint16_t CRC16::calculate(const uint8_t *data, size_t length)
{
    uint16_t crc = INITIAL_VALUE;

    for (size_t i = 0; i < length; i++)
    {
        // Align the byte with the high byte of the CRC.
        crc ^= (data[i] << 8);

        for (int bit = 0; bit < 8; bit++)
        {
            // Check if the most significant bit is 1.
            // If so, left shift the CRC and xor with the polynomial.
            // If not, just perform the left shift.
            if (crc & 0x8000)
                crc = (crc << 1) ^ POLYNOMIAL;
            else
                crc <<= 1;
        }
    }

    // Truncate the result to 16 bits.
    return crc & INITIAL_VALUE;
}