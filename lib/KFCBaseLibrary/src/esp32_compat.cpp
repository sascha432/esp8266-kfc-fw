/**
 * Author: sascha_lammers@gmx.de
 */

#if defined(ESP32)

#include <Arduino.h>
#include <time.h>
#include <LoopFunctions.h>
#include "esp32_compat.h"

//TODO esp32
void analogWrite(uint8_t pin, uint16_t value)
{
}

bool can_yield()
{
    return !xPortInIsrContext();
}

extern "C" {

    uint32_t sntp_update_delay_MS_rfc_not_less_than_15000();

    uint32_t __wrap_sntp_get_sync_interval()
    {
        return sntp_update_delay_MS_rfc_not_less_than_15000();
    }

}

#endif
