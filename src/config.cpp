/**
 * Author: sascha_lammers@gmx.de
 */

//TODO remove

/*

uint8_t WiFi_mode_connected(uint8_t mode, uint32_t *station_ip, uint32_t *ap_ip) {
#if defined(ESP8266)
    struct ip_info ip_info;
    uint8_t connected_mode = 0;

    auto flags = config._H_GET(Config().flags);
    if (flags.wifiMode & mode) { // is any mode active?
        if ((mode & WIFI_STA) && (flags.wifiMode & WIFI_STA) && wifi_station_get_connect_status() == STATION_GOT_IP) { // station connected?
            if (wifi_get_ip_info(STATION_IF, &ip_info) && ip_info.ip.addr) { // verify that is has a valid IP address
                connected_mode |= WIFI_STA;
                if (station_ip) {
                    *station_ip = ip_info.ip.addr;
                }
            }
        }
        if ((mode & WIFI_AP) && (flags.wifiMode & WIFI_AP)) { // AP mode active?
            if (wifi_get_ip_info(SOFTAP_IF, &ip_info) && ip_info.ip.addr) { // verify that is has a valid IP address
                connected_mode |= WIFI_AP;
                if (ap_ip) {
                    *ap_ip = ip_info.ip.addr;
                }
            }
        }
    }
    return connected_mode;
#else
    uint8_t connected_mode = 0;

    auto flags = config._H_GET(Config().flags);
    if (flags.wifiMode & mode) { // is any mode active?

        if ((mode & WIFI_STA) && (flags.wifiMode & WIFI_STA) && WiFi.isConnected()) { // station connected?
            connected_mode |= WIFI_STA;
            if (station_ip) {
                *station_ip = (uint32_t)WiFi.localIP();
            }
        }
        if ((mode & WIFI_AP) && (flags.wifiMode & WIFI_AP)) { // AP mode active?
            connected_mode |= WIFI_AP;
            if (ap_ip) {
                *ap_ip = (uint32_t)WiFi.softAPIP();
            }
        }
    }
    return connected_mode;
#endif
}
*/
