/**
 * Author: sascha_lammers@gmx.de
 */

#include "Arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

void NeoPixel_fillColor(uint8_t *pixels, uint16_t numBytes, uint32_t color);
void NeoPixel_espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes, bool is800KHz);

// #if defined(ESP8266)
// void ICACHE_RAM_ATTR espShow(uint8_t *p, uint8_t *end, uint8_t pix, uint8_t mask, uint32_t time0, uint32_t time1, uint32_t period, const uint32_t gpio_clear_address, const uint32_t gpio_set_address, const uint32_t gpio_clear_mask, const uint32_t gpio_set_mask);
// #else
// void ICACHE_RAM_ATTR espShow(uint8_t pin, uint8_t *p, uint8_t *end, uint8_t pix, uint8_t mask, uint32_t time0, uint32_t time1, uint32_t period);
// #endif

#ifdef __cplusplus
}
#endif
