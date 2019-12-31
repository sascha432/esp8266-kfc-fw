/**
 * Author: sascha_lammers@gmx.de
 */

// support for some esp8266 AT commands that seem useful

#if AT_MODE_SUPPORTED && ESP8266_AT_MODE_SUPPORT

#include <Arduino_compat.h>
#include "async_web_response.h"
#include "kfc_fw_config.h"
#include "plugins.h"
#include "at_mode.h"

// https://www.espressif.com/sites/default/files/documentation/4a-esp8266_at_instruction_set_en.pdf


bool esp8266_at_commands_require_mode(Stream &output, WiFiMode_t mode) {
    if (!(config._H_GET(Config().flags).wifiMode & mode)) {
        output.print(F("ERROR - "));
        if (mode == WIFI_AP) {
            output.print(F("SoftAP"));
        } else if (mode == WIFI_STA) {
            output.print(F("Station mode"));
        } else {
            output.print(F("Station mode and SoftAP"));
        }
        output.println(F(" disabled"));
        return true;
    }
    return false;
}

void esp8266_at_commands_reconfigure_wifi(Stream &output) {
    if (config.reconfigureWiFi()) {
        at_mode_print_ok(output);
    } else {
        output.printf_P(PSTR("ERROR - reconfiguring WiFi failed: %s\n"), config.getLastError());
    }
}

PROGMEM_AT_MODE_HELP_COMMAND_DEF(CWMODE, "CWMODE", "<mode>", "Set WiFi mode. 1=STA, 2=AP, 3=STA+AP", "Display WiFi mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CWLAP, "CWLAP", "List available APs");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CWJAP, "CWJAP", "<ssid>,<password>", "Connect to AP", "Display WiFi connection");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CWQAP, "CWQAP", "Disconnect from AP");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CIPSTATUS, "CIPSTATUS", "Get the connection status");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CIFSR, "CIFSR", "Get IP address");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CWSAP, "CWSAP", "<ssid>,<password>[,<channel>[,<encryption>]]", "Configure SoftAP\nEncryption: 0=OPEN, 2=WPA_PSK, 3=WPA2_PSK, 4=WPA_WPA2_PSK", "Display SoftAP settings");
#if defined(ESP8266)
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CWAPDHCP, "CWAPDHCP", "<0=disable|1=enable>[,<dhcp-lease-start>[,<dhcp-lease-end>]]>", "Configure DHCP server", "Display DHCP server settings");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(GMR, "GMR", "Print firmware version");

class ESP8266ATModePlugin : public PluginComponent {
public:
    ESP8266ATModePlugin() {
        REGISTER_PLUGIN(this, "ESP8266ATModePlugin");
    }

    PGM_P getName() const;

    PluginPriorityEnum_t getSetupPriority() const override {
        return MIN_PRIORITY;
    }

    bool hasAtMode() const override {
        return true;
    }

    void atModeHelpGenerator() override;
    bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
};

static ESP8266ATModePlugin plugin;

PGM_P ESP8266ATModePlugin::getName() const {
    return PSTR("esp8266atmode");
}

void ESP8266ATModePlugin::atModeHelpGenerator() {
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CWMODE));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CWLAP));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CWJAP));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CWQAP));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CIPSTATUS));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CIFSR));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CWSAP));
#if defined(ESP8266)
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CWAPDHCP));
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(GMR));
}

bool ESP8266ATModePlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CWMODE))) {
        if (argc == AT_MODE_QUERY_COMMAND) {
            serial.printf_P(PSTR("+CWMODE:%d\n"), config._H_GET(Config().flags).wifiMode);
        }
        else if (argc != 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            int mode = atoi(argv[0]);
            if (mode >= 1 && mode <= 3) {
                config._H_W_GET(Config().flags).wifiMode = mode;
                esp8266_at_commands_reconfigure_wifi(serial);
            }
            else {
                serial.println(F("ERROR - Invalid mode"));
            }
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CWSAP))) {
        if (esp8266_at_commands_require_mode(serial, WIFI_AP)) {
        }
        else if (argc == AT_MODE_QUERY_COMMAND) {
            serial.printf_P(PSTR("+CWSAP:\"%s\",\"%s\",%d,%d\n"),
                config._H_STR(Config().soft_ap.wifi_ssid),
                config._H_STR(Config().soft_ap.wifi_pass),
                (int)config._H_GET(Config().soft_ap.channel),
                (int)config._H_GET(Config().soft_ap.encryption)
            );
        }
        else if (argc < 2) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            config._H_SET_STR(Config().soft_ap.wifi_ssid, argv[0]);
            config._H_SET_STR(Config().soft_ap.wifi_pass, argv[1]);
            if (argc >= 3) {
                config._H_SET(Config().soft_ap.channel, atoi(argv[2]));
                if (argc >= 4) {
                    config._H_SET(Config().soft_ap.encryption, atoi(argv[3]));
                }
            }
            esp8266_at_commands_reconfigure_wifi(serial);
        }
        return true;
    }
#if defined(ESP8266)
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CWAPDHCP))) {
        if (esp8266_at_commands_require_mode(serial, WIFI_STA)) {
        }
        else if (argc == AT_MODE_QUERY_COMMAND) {
            struct dhcps_lease dhcp_lease;
            if (wifi_softap_get_dhcps_lease(&dhcp_lease)) {
                String start = IPAddress(dhcp_lease.start_ip.addr).toString();
                String end = IPAddress(dhcp_lease.end_ip.addr).toString();
                serial.printf_P(PSTR("+CWAPDHCP:%d,%s,%s\n"), wifi_softap_dhcps_status() == DHCP_STARTED, start.c_str(), end.c_str());
            }
            else {
                serial.println(F("ERROR - Failed to get DHCP server status"));
            }
        }
        else if (argc < 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            int enable = atoi(argv[0]);
            if (enable == 0) {
                config._H_W_GET(Config().flags).softAPDHCPDEnabled = false;
                wifi_softap_dhcps_stop();
                at_mode_print_ok(serial);
            }
            else {
                bool error = false;
                struct dhcps_lease dhcp_lease;
                if (wifi_softap_get_dhcps_lease(&dhcp_lease)) {
                    dhcp_lease.enable = 1;
                    if (argc >= 2) {
                        IPAddress addr;
                        if (addr.fromString(argv[1])) {
                            dhcp_lease.start_ip.addr = (uint32_t)addr;
                            config._H_SET(Config().soft_ap.dhcp_start, addr);
                        } else {
                            error = true;
                            serial.println(F("ERROR - Invalid start address"));
                        }
                        if (argc >= 3) {
                            if (addr.fromString(argv[2])) {
                                dhcp_lease.end_ip.addr = (uint32_t)addr;
                                config._H_SET(Config().soft_ap.dhcp_end, addr);
                            } else {
                                error = true;
                                serial.println(F("ERROR - Invalid end address"));
                            }
                        }
                    }
                    if (!error) {
                        config._H_W_GET(Config().flags).softAPDHCPDEnabled = true;
                        wifi_softap_dhcps_stop();
                        if (wifi_softap_set_dhcps_lease(&dhcp_lease) && wifi_softap_dhcps_start()) {
                            at_mode_print_ok(serial);
                        } else {
                            serial.println(F("ERROR - Failed to configure DHCP server"));
                        }
                    }
                }
                else {
                    serial.println(F("ERROR - Failed to get DHCP server status"));
                }
            }
        }
        return true;
    }
#endif
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CWLAP))) {
        uint8_t count = 15;
        while(AsyncNetworkScanResponse::isLocked() && count--) {
            delay(100);
        }
        if (AsyncNetworkScanResponse::isLocked()) {
            serial.println(F("ERROR - Scan running, try later again"));
        } else {
            AsyncNetworkScanResponse::setLocked();
            int8_t num_networks = WiFi.scanNetworks(false, true);
            for (int8_t i = 0; i < num_networks; i++) {
                serial.printf_P(PSTR("+CWLAP:(%d,\"%s\",%d,\"%s\",%d)\n"), WiFi.encryptionType(i), WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.BSSIDstr(i).c_str(), WiFi.channel(i));
            }
            AsyncNetworkScanResponse::setLocked(false);
            at_mode_print_ok(serial);
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CWJAP))) {
        if (esp8266_at_commands_require_mode(serial, WIFI_STA)) {
        }
        else if (argc == AT_MODE_QUERY_COMMAND) {
            serial.printf_P(PSTR("+CWJAP:\"%s\",\"%s\"\n"), config._H_STR(Config().wifi_ssid), config._H_STR(Config().wifi_pass));
            serial.printf_P(PSTR("+CWJAP: WiFi is %s\n"), WiFi.isConnected() ? PSTR("connected") : PSTR("disconnected"));
        }
        else {
            if (argc >= 1) {
                config._H_SET_STR(Config().wifi_ssid, argv[0]);
                if (argc >= 2) {
                    config._H_SET_STR(Config().wifi_pass, argv[1]);
                }
            }
            esp8266_at_commands_reconfigure_wifi(serial);
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CWQAP))) {
        if (esp8266_at_commands_require_mode(serial, WIFI_STA)) {
        }
        else if (WiFi.isConnected()) {
            WiFi.setAutoReconnect(false);
            WiFi.disconnect(true);
            at_mode_print_ok(serial);
        }
        else {
            serial.println(F("ERROR - WiFi is not connected"));
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CIPSTATUS))) {
        switch (WiFi.status()) {
            case WL_CONNECTED:
                serial.println(F("+CIPSTATUS: Connected"));
                break;
            case WL_IDLE_STATUS:
                serial.println(F("+CIPSTATUS: Idle"));
                break;
            case WL_CONNECT_FAILED:
                serial.println(F("+CIPSTATUS: Connect failed"));
                break;
            case WL_CONNECTION_LOST:
                serial.println(F("+CIPSTATUS: Connection lost"));
                break;
            case WL_DISCONNECTED:
                serial.println(F("+CIPSTATUS: Disconnected"));
                break;
            case WL_NO_SSID_AVAIL:
                serial.println(F("+CIPSTATUS: No SSID available"));
                break;
            case WL_SCAN_COMPLETED:
                serial.println(F("+CIPSTATUS: Scan completed"));
                break;
            case WL_NO_SHIELD:
                serial.println(F("+CIPSTATUS: No WiFi shield"));
                break;
        }
        if (config._H_GET(Config().flags).wifiMode & WIFI_AP) {
            serial.printf_P(PSTR("+CIPSTATUS: %d station(s) connected\n"), WiFi.softAPgetStationNum());
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(CIFSR))) {
        if (esp8266_at_commands_require_mode(serial, WIFI_STA)) {
        }
        else if (WiFi.isConnected()) {
            String localIp = WiFi.localIP().toString();
            serial.printf_P(PSTR("+CIFSR=%s\n"), localIp.c_str());
        }
        else {
            serial.println(F("ERROR - WiFi is not connected"));
        }
        return true;
    }
    else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(GMR))) {
        config.printVersion(serial);
        return true;
    }
    return false;
}

#endif
