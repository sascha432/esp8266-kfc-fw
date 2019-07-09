/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

uint16_t crc16_compute(uint16_t crc, uint8_t a);
uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t len);
uint16_t crc16_calc(const uint8_t *data, size_t len);

constexpr uint16_t constexpr_crc16_compute(uint16_t crc, int i) {
    return i < 8 ? constexpr_crc16_compute((crc & 1) ? ((crc >> 1) ^ 0xa001) : (crc >> 1), i + 1) : crc;
}

uint16_t constexpr constexpr_crc16_update(uint16_t crc, const uint8_t *data, size_t len) {
    return len == 0 ? crc : constexpr_crc16_update(constexpr_crc16_compute(crc ^ *data, 0), data + 1, len - 1);
}

uint16_t constexpr constexpr_crc16_calc(const uint8_t *data, size_t len) {
    return constexpr_crc16_update(~0, data, len);
}
