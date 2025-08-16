//
// Created by Jackson Hu on 17/8/2025.
//

#include "lplora_utils.hpp"

uint16_t lplora_utils::calc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc = (crc >> 8) ^ CRC16_KERMIT_LUT[(crc & 0xff) ^ data[i]];
    }

    return crc;
}
