/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

uint16_t _crc16_update(uint16_t crc, uint8_t a);
uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t len);
uint16_t crc16_calc(const uint8_t *data, size_t len);

uint16_t constexpr constexpr__crc16_update(uint16_t crc, uint8_t a) {
    crc ^= a;
    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xA001;
        } else {
            crc = (crc >> 1);
        }
    }
    return crc;
}

uint16_t constexpr constexpr_crc16_update(uint16_t crc, const uint8_t *data, size_t len) {
    while(len--) {
        crc = constexpr__crc16_update(crc, *data++);
    }
    return crc;
}

uint16_t constexpr constexpr_crc16_calc(const uint8_t *data, size_t len) {
    return constexpr_crc16_update(~0, data, len);
}    