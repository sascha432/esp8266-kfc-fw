/**
 * Author: sascha_lammers@gmx.de
 */

#if defined(ESP8266)

#include <Arduino.h>
#include "global.h"
#include "esp8266_compat.h"

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020701

    // settimeofday_cb() working again?

#elif ARDUINO_ESP8266_VERSION_COMBINED == 0x020603

    #include "esp_settimeofday_cb.h"

#endif

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x030000

    #include <LwipDhcpServer.h>

    bool wifi_softap_get_dhcps_lease(struct dhcps_lease *please) {
        return dhcpSoftAP.get_dhcps_lease(please);
    }

    bool wifi_softap_set_dhcps_lease(struct dhcps_lease *please) {
        return dhcpSoftAP.set_dhcps_lease(please);
    }

#endif


#endif
