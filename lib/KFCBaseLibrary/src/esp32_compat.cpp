/**
 * Author: sascha_lammers@gmx.de
 */

#if defined(ESP32)

#include <Arduino.h>
#include <time.h>
#include "esp32_compat.h"

enum dhcp_status wifi_station_dhcpc_status(void)  {
    return DHCP_STOPPED;
}

//TODO esp32
void analogWrite(uint8_t pin, uint16_t value) {
}

#include "esp_settimeofday_cb.h"

#endif
