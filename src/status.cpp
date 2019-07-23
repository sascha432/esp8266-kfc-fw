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
        if (WiFi.isConnected()) {
        // if (wifi_station_get_connect_status() == STATION_GOT_IP) {
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

#if defined(ESP32)
String WiFi_get_tx_power() {
    switch(WiFi.getTxPower()) {
        case WIFI_POWER_19_5dBm: return F("19.5dBm");
        case WIFI_POWER_19dBm: return F("19dBm");
        case WIFI_POWER_18_5dBm: return F("18.5dBm");
        case WIFI_POWER_17dBm: return F("17dBm");
        case WIFI_POWER_15dBm: return F("15dBm");
        case WIFI_POWER_13dBm: return F("13dBm");
        case WIFI_POWER_11dBm: return F("11dBm");
        case WIFI_POWER_8_5dBm: return F("8.5dBm");
        case WIFI_POWER_7dBm: return F("7dBm");
        case WIFI_POWER_5dBm: return F("5dBm");
        case WIFI_POWER_2dBm: return F("2dBm");
        case WIFI_POWER_MINUS_1dBm: return F("-1dBm");
    }
    return F("Unknown");
}
#endif

void WiFi_get_status(Print &out) {
    uint8_t mode = WiFi.getMode();
    if (mode & WIFI_STA) {

        out.printf_P(PSTR(HTML_S(strong) "Station:" HTML_E(strong) HTML_S(br)));
#if defined(ESP8266)
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
                wifi_country_t country;
                if (wifi_get_country(&country)) {
                    out.printf_P(PSTR(", country %.2s"), country.cc);
                }
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
            if (wifi_station_dhcpc_status() == DHCP_STARTED) {
                out.print(F(HTML_S(br) "DHCP client running"));
            }
#elif defined(ESP32)
            if (WiFi.isConnected()) {
                out.printf_P(PSTR("Connected, signal strength %d dBm, channel %d, TX power %s"), WiFi.RSSI(), WiFi.channel(), WiFi_get_tx_power().c_str());
                wifi_country_t country;
                if (esp_wifi_get_country(&country) == ESP_OK) {
                    out.printf_P(PSTR(", country %.2s"), country.cc);
                }
            } else {
                switch(WiFi.status()) {
                    case WL_NO_SSID_AVAIL:
                        out.print(F("No SSID available"));
                        break;
                    case WL_CONNECT_FAILED:
                        out.print(F("Connect failed"));
                        break;
                    case WL_CONNECTION_LOST:
                        out.print(F("Connection lost"));
                        break;
                    case WL_IDLE_STATUS:
                        out.print(F("Idle"));
                        break;
                    default:
                        out.print(F("Disconnected"));
                        break;
                }
            }
            if (config._H_GET(Config().flags).stationModeDHCPEnabled) {
                out.print(F(HTML_S(br) "DHCP client running"));
            }
#else
#error Platform not supported
#endif
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

#if defined(ESP8266)
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
#elif defined(ESP32)
    if (mode & WIFI_AP) {
        out.print(F(HTML_S(strong) "Access point:" HTML_E(strong) HTML_S(br)));
        out.print(F("IP Address "));
        WiFi.softAPIP().printTo(out);
        out.printf_P(PSTR(HTML_S(br) "Clients connected %d" HTML_S(br)), WiFi.softAPgetStationNum());

        //TODO esp32
        // see code for esp8266

    }
#else
#error Platform not supported
#endif
}


void WiFi_Station_SSID(Print &out) {
    if (WiFi.isConnected()) {
    //if (wifi_station_get_connect_status() == STATION_GOT_IP) {
        out.print(WiFi.SSID());
    } else {
        out.print(config._H_STR(Config().wifi_ssid));
    }
}

void WiFi_SoftAP_SSID(Print &out) {
#if defined(ESP32)
    if (false) {
        //TODO esp32
    }
#elif defined(ESP8266)
    softap_config config;
    if (wifi_softap_get_config(&config)) {
        out.write(config.ssid, config.ssid_len);
        if (config.ssid_hidden) {
            out.print(F(" (" HTML_S(i) "HIDDEN" HTML_E(i) ")"));
        }
    }
#endif
    else {
        out.print(F("*Unavailable*"));
    }
}
