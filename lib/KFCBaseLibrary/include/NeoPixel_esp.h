/**
 * Author: sascha_lammers@gmx.de
 */

#include "Arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

void NeoPixel_setColor(uint8_t *pixels, uint32_t color);
void NeoPixel_fillColor(uint8_t *pixels, uint16_t numBytes, uint32_t color);

void ICACHE_RAM_ATTR NeoPixel_espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes, boolean is800KHz);

#ifdef __cplusplus
}
#endif
