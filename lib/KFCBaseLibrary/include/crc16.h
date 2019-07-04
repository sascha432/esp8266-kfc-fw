/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

uint16_t _crc16_update(uint16_t crc, uint8_t a);
uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t len);
uint16_t crc16_calc(const uint8_t *data, size_t len);
