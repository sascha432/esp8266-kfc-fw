/**
 * Author: sascha_lammers@gmx.de
 */

#if defined(ESP8266)

#include <Arduino.h>
#include "global.h"
#include "esp8266_compat.h"

#if ARDUINO_ESP8266_MAJOR == 2 && ARDUINO_ESP8266_MINOR == 6

    #warning not supported anymore

#endif

#if ARDUINO_ESP8266_MAJOR >= 3

    #include <LwipDhcpServer.h>

    bool wifi_softap_get_dhcps_lease(struct dhcps_lease *please) {
        return dhcpSoftAP.get_dhcps_lease(please);
    }

    bool wifi_softap_set_dhcps_lease(struct dhcps_lease *please) {
        return dhcpSoftAP.set_dhcps_lease(please);
    }

#endif

#endif
