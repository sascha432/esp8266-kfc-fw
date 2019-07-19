 /**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include "status.h"
#include "kfc_fw_config.h"
#include "progmem_data.h"
#include "kfc_fw_config.h"

// TODO make sure everything gets escaped with htmlEntities()

void WiFi_get_address(Print &out) {

    uint8_t mode = WiFi.getMode();
    if (mode & WIFI_STA) {
        if (mode & WIFI_AP) {
            out.print(F("Client IP "));
        }
        if (wifi_station_get_connect_status() == STATION_GOT_IP) {
            WiFi.localIP().printTo(out);
        } else {
            out.print(F(HTML_S(i) "Offline" HTML_E(i)));
        }

    }
    if (mode & WIFI_AP) {
        if (mode & WIFI_STA) {
            out.print(F(", Access Point "));
        }
        WiFi.softAPIP().printTo(out);
    }
}

void WiFi_get_status(Print &out) {
    uint8_t mode = WiFi.getMode();
    if (mode & WIFI_STA) {

// struct dhcps_lease {
//     bool enable;
//     struct ip_addr start_ip;
//     struct ip_addr end_ip;
// };

// wifi_softap_get_station_info


// #define STATION_IF      0x00
// #define SOFTAP_IF       0x01
// struct ip_info {
//     struct ip_addr ip;
//     struct ip_addr netmask;
//     struct ip_addr gw;
// };

// wifi_get_ip_info(SOFTAP_IF, ip_info);

// bool wifi_get_ip_info(uint8 if_index, struct ip_info *info);
// bool wifi_set_ip_info(uint8 if_index, struct ip_info *info);

        out.printf_P(PSTR(HTML_S(strong) "Station:" HTML_E(strong) HTML_S(br)));
        switch (wifi_station_get_connect_status()) {
            case STATION_GOT_IP:
                out.printf_P(PSTR("Connected, signal strength %d dBm, channel %d, PHY mode "), WiFi.RSSI(), WiFi.channel());
                switch(WiFi.getPhyMode()) {
                    case WIFI_PHY_MODE_11B:
                        out.print(F("802.11b"));
                        break;
                    case WIFI_PHY_MODE_11G:
                        out.print(F("802.11g"));
                        break;
                    case WIFI_PHY_MODE_11N:
                        out.print(F("802.11n"));
                        break;
                }
                // wifi_country_t country;
                // if (wifi_get_country(&country)) {
                //     out.printf_P(PSTR(", WiFi Country %s"), country.cc);
                // }
                break;
            case STATION_NO_AP_FOUND:
                out.print(F("No SSID available"));
                break;
            case STATION_CONNECTING:
                out.print(F("Connecting"));
                break;
            case STATION_CONNECT_FAIL:
                out.print(F("Connect failed"));
                break;
            case STATION_WRONG_PASSWORD:
                out.print(F("Wrong password"));
                break;
            case STATION_IDLE:
                out.print(F("Idle"));
                break;
            default:
                out.print(F("Disconnected"));
                break;
            }
            out.print(F(HTML_S(br) "IP Address/Network "));
            WiFi.localIP().printTo(out);
            out.print(FSPGM(slash));
            WiFi.subnetMask().printTo(out);
            out.print(F(HTML_S(br) "Gateway "));
            WiFi.gatewayIP().printTo(out);
            out.print(F(", DNS "));
            WiFi.dnsIP().printTo(out);
            out.print(FSPGM(comma_));
            WiFi.dnsIP(1).printTo(out);
    }

    if (mode & WIFI_AP_STA) {
        out.print(F(HTML_NEW_2COL_ROW));
    }
    if (mode & WIFI_AP) {
        ip_info if_cfg;

        out.print(F(HTML_S(strong) "Access point:" HTML_E(strong) HTML_S(br)));

        out.print(F("IP Address "));
        WiFi.softAPIP().printTo(out);
        out.printf_P(PSTR(HTML_S(br) "Clients connected %d"), WiFi.softAPgetStationNum());
        softap_config config;
        if (wifi_softap_get_config(&config)) {
            out.printf_P(PSTR(" out of %d"), config.max_connection);
        }

        if (wifi_get_ip_info(SOFTAP_IF, &if_cfg)) {
            out.print(F(HTML_S(br) "IP Address/Network "));
            IPAddress(if_cfg.ip.addr).printTo(out);
            out.print(FSPGM(slash));
            IPAddress(if_cfg.netmask.addr).printTo(out);
            out.print(F(HTML_S(br) "Gateway "));
            IPAddress(if_cfg.gw.addr).printTo(out);
            out.print(F(" " HTML_S(br)));
        }
        if (wifi_softap_dhcps_status() == DHCP_STOPPED) {
            out.print(F("DHCP server not running"));
        } else {
            dhcps_lease please;
            if (wifi_softap_get_dhcps_lease(&please)) {
                out.print(F(HTML_S(br) "DHCP server lease range "));
                IPAddress(please.start_ip.addr).printTo(out);
                out.print(F(" - "));
                IPAddress(please.end_ip.addr).printTo(out);
            }
        }

        out.print(F(HTML_NEW_2COL_ROW HTML_S(strong) "Connected stations:" HTML_E(strong) HTML_S(br)));

        station_info *info;
        info = wifi_softap_get_station_info();
        if (info == NULL) {
            out.print(F("No clients connected"));
        } else {
            while(info != NULL) {

                out.print(F(HTML_S(span style="width: 200px; float: left")));
                IPAddress(info->ip.addr).printTo(out);
                out.print(F(HTML_E(span) HTML_S(span style="width: 200px; float: left")));
                out.printf_P(PSTR(MACSTR HTML_E(span) HTML_S(br)), MAC2STR(info->bssid));

                info = STAILQ_NEXT(info, next);
            }
        }
        wifi_softap_free_station_info();
    }
}


void WiFi_Station_SSID(Print &out) {
    if (wifi_station_get_connect_status() == STATION_GOT_IP) {
        out.print(WiFi.SSID());
    } else {
        out.print(config._H_STR(Config().wifi_ssid));
    }
}

void WiFi_SoftAP_SSID(Print &out) {
    softap_config config;
    if (wifi_softap_get_config(&config)) {
        for (uint8_t i = 0; i < config.ssid_len; i++) {
            out.write(config.ssid[i]);
        }
        if (config.ssid_hidden) {
            out.print(F(" (" HTML_S(i) "HIDDEN" HTML_E(i) ")"));
        }
    } else {
        out.print(F("*Unavailable*"));
    }
}
