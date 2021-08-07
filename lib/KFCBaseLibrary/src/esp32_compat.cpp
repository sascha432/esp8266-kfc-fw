/**
 * Author: sascha_lammers@gmx.de
 */

#if defined(ESP32)

#include <Arduino.h>
#include <time.h>
#include "esp32_compat.h"

//TODO esp32
void analogWrite(uint8_t pin, uint16_t value)
{
}

bool can_yield()
{
    return true;
}

#include "esp_settimeofday_cb.h"

#endif
