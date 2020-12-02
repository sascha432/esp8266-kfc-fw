 /**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include "status.h"
#include "kfc_fw_config.h"

void WiFi_get_address(Print &out)
{
    uint8_t mode = WiFi.getMode();
    if (mode & WIFI_STA) {
        if (mode & WIFI_AP) {
            out.print(F("Client IP "));
        }
        if (WiFi.isConnected()) {
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

void WiFi_get_status(Print &out)
{
    uint8_t mode = WiFi.getMode();
    if (mode & WIFI_STA) {

        out.printf_P(PSTR(HTML_S(strong) "Station:" HTML_E(strong) HTML_S(br)));
#if defined(ESP8266)
        switch (wifi_station_get_connect_status()) {
            case STATION_GOT_IP:
                out.printf_P(PSTR("Connected, signal strength %d dBm, channel %u, mode "), WiFi.RSSI(), WiFi.channel());
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
                out.printf_P(PSTR("Connected, signal strength %d dBm, channel %u, TX power %s"), WiFi.RSSI(), WiFi.channel(), WiFi_get_tx_power().c_str());

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

            if (System::Flags::getConfig().is_softap_dhcpd_enabled) {
                out.print(F(HTML_S(br) "DHCP client running"));
            }
#else
#error Platform not supported
#endif
            out.print(F(HTML_S(br) "IP Address/Network "));
            WiFi.localIP().printTo(out);
            out.print('/');
            WiFi.subnetMask().printTo(out);
            out.print(F(HTML_S(br) "Gateway "));
            WiFi.gatewayIP().printTo(out);
            out.print(F(", DNS "));
            WiFi.dnsIP().printTo(out);
            out.print(F(", "));
            WiFi.dnsIP(1).printTo(out);
    }

    if (mode & WIFI_AP_STA) {
        out.print(F(HTML_S(br)));
    }

#if defined(ESP8266)
    if (mode & WIFI_AP) {
        ip_info if_cfg;

        out.print(F(HTML_S(br) HTML_S(strong) "Access point:" HTML_E(strong) HTML_S(br)));

        softap_config config;
        if (!wifi_softap_get_config(&config)) {
            config.max_connection = 4;
        }
        out.printf_P(PSTR("Clients connected %u out of %u" HTML_S(br)), WiFi.softAPgetStationNum(), config.max_connection);

        if (wifi_get_ip_info(SOFTAP_IF, &if_cfg)) {
            out.print(F("IP Address/Network "));
            IPAddress(if_cfg.ip.addr).printTo(out);
            out.print('/');
            IPAddress(if_cfg.netmask.addr).printTo(out);
            out.print(F(HTML_S(br) "Gateway "));
            IPAddress(if_cfg.gw.addr).printTo(out);
            out.print(F(HTML_S(br)));
        }
        if (wifi_softap_dhcps_status() == DHCP_STOPPED) {
            out.print(F("DHCP server not running"));
        } else {
            dhcps_lease please;
            if (wifi_softap_get_dhcps_lease(&please)) {
                out.print(F("DHCP server lease range "));
                IPAddress(please.start_ip.addr).printTo(out);
                out.print(F(" - "));
                IPAddress(please.end_ip.addr).printTo(out);
            }
        }

        out.print(F(HTML_S(br) HTML_S(br) HTML_S(strong) "Connected stations:" HTML_E(strong) HTML_S(br)));

        station_info *info;
        info = wifi_softap_get_station_info();
        if (info == NULL) {
            out.print(F("No clients connected"));
        } else {
            while(info != NULL) {

                out.print(F(HTML_S(span style\5\4width: 150px; float: left\4)));
                IPAddress(info->ip.addr).printTo(out);
                out.print(F(HTML_E(span) HTML_S(span style\5\4width: 170px; float: left\4)));
                printMacAddress(info->bssid, out);
                out.print(F(HTML_E(span) HTML_S(br)));

                info = STAILQ_NEXT(info, next);
            }
        }
        wifi_softap_free_station_info();
    }
#elif defined(ESP32)
    if (mode & WIFI_AP) {

        tcpip_adapter_ip_info_t ip;
        if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip) != ESP_OK) {
            memset(&ip, 0, sizeof(ip));
        }

        out.print(F(HTML_S(strong) "Access point:" HTML_E(strong) HTML_S(br)));

        wifi_sta_list_t clients;
        if (esp_wifi_ap_get_sta_list(&clients) != ESP_OK) {
            clients.num = 0;
        }

        out.printf_P(PSTR("Clients connected %u out of %u" HTML_S(br)), clients.num, ESP_WIFI_MAX_CONN_NUM);

        out.print(F("IP Address/Network "));
        IPAddress(ip.ip.addr).printTo(out);
        out.print('/');
        IPAddress(ip.netmask.addr).printTo(out);
        out.print(F(HTML_S(br) "Gateway "));
        IPAddress(ip.gw.addr).printTo(out);
        out.print(F(HTML_S(br)));

        tcpip_adapter_dhcp_status_t dhcps_info;
        if (tcpip_adapter_dhcps_get_status(TCPIP_ADAPTER_IF_AP, &dhcps_info) == ESP_OK && dhcps_info == TCPIP_ADAPTER_DHCP_STARTED) {
            dhcps_lease_t lease;
            if (tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_GET, TCPIP_ADAPTER_REQUESTED_IP_ADDRESS, &lease, sizeof(lease)) == ESP_OK) {
                out.print(F("DHCP server lease range "));
                IPAddress(lease.start_ip.addr).printTo(out);
                out.print(F(" - "));
                IPAddress(lease.end_ip.addr).printTo(out);
            }
        } else {
            out.print(F("DHCP server not running"));
        }

        out.print(F(HTML_S(br) HTML_S(br) HTML_S(strong) "Connected stations:" HTML_E(strong) HTML_S(br)));

        if (clients.num == 0) {
            out.print(F("No clients connected"));
        } else {
            tcpip_adapter_sta_list_t tcp_sta_list;
	        if (tcpip_adapter_get_sta_list(&clients, &tcp_sta_list) != ESP_OK) {
                tcp_sta_list.num = 0;
            }

            int index = 0;
            while(index < clients.num) {
                auto &station = clients.sta[index];

                out.print(F(HTML_S(span style\5\4width: 150px; float: left\4)));
                if (index < tcp_sta_list.num) {
                    IPAddress(tcp_sta_list.sta[index].ip.addr).printTo(out);
                } else {
                    out.print(F("N/A"));
                }
                out.print(F(HTML_E(span) HTML_S(span style\5\4width: 170px; float: left\4)));
                out.print(mac2String(station.mac));
                out.print(F(HTML_E(span) HTML_S(span style\5\4width: 80px; float: left\4)));
                out.printf_P(PSTR("%d dBm" HTML_E(span) HTML_S(br)), station.rssi);

                index++;
            }
        }

    }
#else
#error Platform not supported
#endif

#if ENABLE_DEEP_SLEEP
    if (resetDetector.hasWakeUpDetected() && ResetDetectorPlugin::_deepSleepWifiTime != ResetDetectorPlugin::kDeepSleepDisabled) {
        out.print(F(HTML_S(br) HTML_S(br) HTML_S(strong) "Quick connect:" HTML_E(strong) HTML_S(br)));
        out.printf_P(PSTR("WiFi connected established after %ums" HTML_S(br)), ResetDetectorPlugin::_deepSleepWifiTime);
    }
#endif
}


void WiFi_Station_SSID(Print &out)
{
    if (WiFi.isConnected()) {
        out.print(WiFi.SSID());
    } else {
        out.print(config._H_STR(MainConfig().network.WiFiConfig._ssid));
    }
}

void WiFi_SoftAP_SSID(Print &out)
{
#if defined(ESP32)
    wifi_config_t _config;
    wifi_ap_config_t &config = _config.ap;
    if (esp_wifi_get_config(ESP_IF_WIFI_AP, &_config) == ESP_OK) {
#elif defined(ESP8266)
    softap_config config;
    if (wifi_softap_get_config(&config)) {
#else
#error Platform not supported
#endif
        out.printf_P(PSTR("%*.*s"), config.ssid_len, config.ssid_len, config.ssid);
        if (config.ssid_hidden) {
            out.print(F(" (" HTML_S(i) "HIDDEN" HTML_E(i) ")"));
        }
    }
    else {
        out.print(F("*Unavailable*"));
    }
}
