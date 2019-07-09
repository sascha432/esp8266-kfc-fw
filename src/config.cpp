/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EEPROM.h>
#include <PrintString.h>
#include <KFCTimezone.h>
#include <time.h>
#include <OSTimer.h>
#include <LoopFunctions.h>
#include <crc16.h>
#include "progmem_data.h"
#include "fs_mapping.h"
#include "misc.h"
#include "session.h"
#include "kfc_fw_config.h"
#include "reset_detector.h"
#include "build.h"

PROGMEM_STRING_DEF(default_password_warning, "WARNING! Default password has not been changed");
PROGMEM_STRING_DEF(safe_mode_enabled, "SAFE MODE enabled");
PROGMEM_STRING_DEF(SPIFF_configuration_backup, "/configuration.backup");
PROGMEM_STRING_DEF(SPIFF_system_stats, "/system.stats");

KFCFWConfiguration config;
String last_error;

const char  *WiFi_encryption_type(uint8_t type) {
    return type == ENC_TYPE_NONE ? str_P(F("open")) :
        type == ENC_TYPE_WEP ? str_P(F("WEP")) :
            type == ENC_TYPE_TKIP ? str_P(F("WPA/PSK")) :
                type == ENC_TYPE_CCMP ? str_P(F("WPA2/PSK")) :
                    type == ENC_TYPE_AUTO ? str_P(F("auto")) : str_P(F("N/A"));
}

uint16_t eeprom_crc(uint8_t *eeprom, unsigned int length) {
    uint16_t crc = ~0;
    for (unsigned short index = sizeof(crc); index < length; ++index) {
        crc = crc16_compute(crc, eeprom[index]);
    }
    return crc;
}

#if SYSTEM_STATS
SystemStats _systemStats;
static ulong system_stats_last_runtime_update = 0;
static ulong system_stats_next_update = 0;

void  system_stats_display(String prefix, Stream &output) {
    output.print(prefix);
    output.printf_P(PSTR("Total runtime in minutes: %lu\n"), (long)(system_stats_get_runtime() / 60L));
    output.print(prefix);
    output.printf_P(PSTR("Reboot counter: %u\n"), _systemStats.reboot_counter);
    output.print(prefix);
    output.printf_P(PSTR("Reset counter: %u\n"), _systemStats.reset_counter);
    output.print(prefix);
    output.printf_P(PSTR("Crash counter: %u\n"), _systemStats.crash_counter);
    output.print(prefix);
    output.printf_P(PSTR("WiFi reconnect counter: %u\n"), _systemStats.station_reconnects);
    output.print(prefix);
    output.printf_P(PSTR("AP client connect counter: %u\n"), _systemStats.ap_clients_connected);
    output.print(prefix);
    output.printf_P(PSTR("Ping success: %lu\n"), _systemStats.ping_success);
    output.print(prefix);
    output.printf_P(PSTR("Ping timeout: %lu\n"), _systemStats.ping_timeout);
}

#define SYSTEM_STATS_CRC_OFS (uint8_t)sizeof(_systemStats.crc)

uint16_t  system_stats_crc() {
    return eeprom_crc((uint8_t *)&_systemStats + SYSTEM_STATS_CRC_OFS, _systemStats.size - SYSTEM_STATS_CRC_OFS);
}

ulong  system_stats_get_runtime(ulong *runtime_since_reboot) {
    ulong _current_runtime = millis() / 1000L;
    if (_current_runtime < system_stats_last_runtime_update) { // millis() overflow, happens every 46 days
        system_stats_last_runtime_update -= 0xffffffffUL / 1000UL;
    }
    if (runtime_since_reboot != NULL) {
        *runtime_since_reboot = _current_runtime;
    }
    return _systemStats.runtime + _current_runtime - system_stats_last_runtime_update;
}

void system_stats_update(bool open_eeprom) {

    debug_println(F("Saving system stats..."));

    ulong current_runtime;
    _systemStats.runtime = system_stats_get_runtime(&current_runtime);
    system_stats_last_runtime_update = current_runtime;

    if (open_eeprom) {
        EEPROM.begin(sizeof(_systemStats));
    }
    _systemStats.size = sizeof(_systemStats);
    _systemStats.crc = system_stats_crc();
    EEPROM.put(0, _systemStats);
    if (open_eeprom) {
        EEPROM.end();
    }

#if SPIFFS_SUPPORT
    File file = SPIFFS.open(FSPGM(SPIFF_system_stats), file_mode_write());
    if (file) {
        file.write((uint8_t *)&_systemStats, sizeof(_systemStats));
        file.close();
    }
#endif

    system_stats_next_update = current_runtime + SYSTEM_STATS_UPDATE_INTERVAL;
}

void system_stats_read() {
    uint16_t crc;

    bzero(&_systemStats, sizeof(_systemStats));

    EEPROM.begin(sizeof(_systemStats));
    EEPROM.get(0, _systemStats);
    EEPROM.end();
    crc = system_stats_crc();
    if (_systemStats.crc != crc) {

// #if SPIFFS_SUPPORT
//         File file = SPIFFS.open(FSPGM(SPIFF_system_stats), file_mode_read());
//         if (file) {
//             bzero(&_systemStats, sizeof(_systemStats));
//             file.read((uint8_t *)&_systemStats, sizeof(_systemStats));
//             file.close();
//             crc = system_stats_crc();
//         }
// #endif

        if (_systemStats.crc != crc) {
            bzero(&_systemStats, sizeof(_systemStats));
            _systemStats.size = sizeof(_systemStats);
            _systemStats.reboot_counter = 1;
            _systemStats.runtime = millis() / 1000UL;
            system_stats_last_runtime_update = _systemStats.runtime;
        }
    }
}

#endif

class BlinkLEDTimer : public OSTimer {  // PIN is inverted
public:
    BlinkLEDTimer() : OSTimer() {
        _pin = -1;
    }

    virtual void run() override {
        digitalWrite(_pin, _pattern.test(_counter++ % _pattern.size()) ? LOW : HIGH);
    }

    void set(uint32_t delay, int8_t pin, dynamic_bitset &pattern) {
        _pattern = pattern;
        if (_pin != pin) {
            _pin = pin;
            pinMode(_pin, OUTPUT);
            analogWrite(pin, 0);
            digitalWrite(pin, HIGH);
        }
        _counter = 0;
        startTimer(delay, true);
    }

    void detach() {
        if (_pin != -1) {
            analogWrite(_pin, 0);
            digitalWrite(_pin, HIGH);
        }
        OSTimer::detach();
    }

private:
    int8_t _pin;
    uint16_t _counter;
    dynamic_bitset _pattern;
};

static BlinkLEDTimer *led_timer = new BlinkLEDTimer();

void blink_led(int8_t pin, int delay, dynamic_bitset &pattern) {

    MySerial.printf_P(PSTR("blink_led pin %d, pattern %s, delay %d\n"), pin, pattern.toString().c_str(), delay);

    led_timer->detach();
    led_timer->set(delay, pin, pattern);
}

void config_set_blink(uint16_t delay, int8_t pin) { // PIN is inverted

    auto flags = config._H_GET(Config().flags);

    if (pin == -1) {
        auto ledPin = config._H_GET(Config().led_pin);
        debug_printf_P(PSTR("Using configured LED type %d, PIN %d, blink %d\n"), flags.ledMode, ledPin, delay);
        pin = (flags.ledMode != MODE_NO_LED && ledPin != -1) ? ledPin : -1;
    } else {
        debug_printf_P(PSTR("PIN %d, blink %d\n"), pin, delay);
    }

    led_timer->detach();

    // reset pin
    analogWrite(pin, 0);
    digitalWrite(pin, LOW);

    if (pin >= 0) {
        if (delay == BLINK_OFF) {
            digitalWrite(pin, HIGH);
        } else if (delay == BLINK_SOLID) {
            digitalWrite(pin, LOW);
        } else {
            dynamic_bitset pattern;
            if (delay == BLINK_SOS) {
                pattern.setMaxSize(24);
                pattern.setValue(0xcc1c71d5);
                delay = 200;
            } else {
                pattern.setMaxSize(2);
                pattern = 0b10;
                delay = _max(50, _min(delay, 5000));
            }
            led_timer->set(delay, pin, pattern);
        }
    }
}

void  config_setup() {
#if DEBUG
#define __DEBUG_CFS_APPEND " DEBUG MODE ENABLED"
#else
#define __DEBUG_CFS_APPEND ""
#endif
    Logger_notice(F("Starting KFCFW %s"), KFCFWConfiguration::getFirmwareVersion().c_str());
    rng.begin(str_P(F("KFC FW " FIRMWARE_VERSION_STR " " __BUILD_NUMBER)));
    rng.stir((const uint8_t *)WiFi.macAddress().c_str(), WiFi.macAddress().length());

    debug_printf_P(PSTR("Size of configuration: %d\n"), sizeof(config));
    config_set_blink(BLINK_FLICKER);
    rng.stir((const uint8_t *)&config, sizeof(config));
    LoopFunctions::add(config_loop);
}

void config_loop() {
    rng.loop();
#if SYSTEM_STATS || SYSLOG
    ulong seconds = millis() / 1000L;
#  if SYSTEM_STATS
    if (seconds > system_stats_next_update) {
        system_stats_update();
    }
#  endif
#endif
}

bool config_read(bool init, size_t ofs) {
    bool success = true;
    debug_printf_P(PSTR("Reading configuration, offset %u\n"), ofs);

    if (!config.read()) {
        config.restoreFactorySettings();
    }

//     if ((config.crc != crc) || (config.ident != FIRMWARE_IDENT)) {
//         success = false;
// #if SPIFFS_SUPPORT
//         MySerial.println(F("EEPROM CRC mismatch, trying to read backup configuration from file system"));
//         String filename = FPSTR(config_filename);
//         File file = SPIFFS.open(filename, file_mode_read());
// #if DEBUG
//         if (!file) {
//             debug_printf_P(PSTR("Failed to open %s\n"), filename.c_str());
//         }
//         if (file.size() != sizeof(config)) {
//             debug_printf_P(PSTR("File size mismatch %d != %d\n"), file.size(), sizeof(config));
//         }
// #endif
//         if (file && file.size() == sizeof(config)) {
//             if (file.readBytes((char *)&config, sizeof(config)) == sizeof(config)) {
//                 crc = config_crc();
//                 if ((config.crc == crc) && (config.ident == FIRMWARE_IDENT)) {
//                     config_write();
//                 }
//             }
//         }
// #endif
//         if ((config.crc != crc) || (config.ident != FIRMWARE_IDENT)) {
//             MySerial.println(F("Failed to load EEPROM settings, using factory defaults"));
//             _Config.restoreFactorySettings();
//         }
//     }
    auto &version = config._H_W_GET(Config().version);
    if (FIRMWARE_VERSION > version) {
        PrintString message;
        message.printf_P(PSTR("Upgrading EEPROM configuration from %d.%d.%d to " FIRMWARE_VERSION_STR), (version >> 16), (version >> 8) & 0xff, (version & 0xff));
        Logger_warning(message);
        debug_println(message);
        config_write();
    }
    return success;
}

void config_write(bool factory_reset, size_t ofs) {

    debug_printf_P(PSTR("Writing configuration, offset %u, factory reset %d\n"), ofs, factory_reset);

    if (factory_reset) {
        config.restoreFactorySettings();
    } else {
        auto &flags = config._H_W_GET(Config().flags);
        flags.isFactorySettings = false;
    }

    config.write();
#if DEBUG
    //config.dumpEEPROM(MySerial);
    config.dump(MySerial);
#endif

#if SYSTEM_STATS
    if (ofs == 0) {
        system_stats_update(false);
    }
#endif

    // File file = SPIFFS.open(FSPGM(SPIFF_configuration_backup), file_mode_write());
    // if (file) {
    //     EEPROM.begin(EEPROM.length());
    //     file.write(EEPROM.getConstDataPtr(), config.getEEPROMSize());
    //     file.close();
    //     EEPROM.end();
    // }
}

void config_version() {
    MySerial.printf_P(PSTR("KFC Firmware %s\nFlash size %s\n"), KFCFWConfiguration::getFirmwareVersion().c_str(), formatBytes(ESP.getFlashChipRealSize()).c_str());
}

void config_info() {
    config_version();

    auto flags = config._H_GET(Config().flags);

    if (flags.wifiMode & WIFI_AP) {
        MySerial.printf_P(PSTR("AP Mode SSID %s\n"), config._H_STR(Config().soft_ap.wifi_ssid));
    } else if (flags.wifiMode & WIFI_STA) {
        MySerial.printf_P(PSTR("Station Mode SSID %s\n"), config._H_STR(Config().wifi_ssid));
    }
    if (flags.isFactorySettings) {
        MySerial.println(F("Running on factory settings."));
    }
    if (flags.isDefaultPassword) {
        String str = FSPGM(default_password_warning);
        Logger_security(str);
        MySerial.println(str);
    }
    MySerial.printf_P(PSTR("Device %s ready!\n"), config._H_STR(Config().device_name));
#if AT_MODE_SUPPORTED
    if (flags.atModeEnabled) {
        MySerial.println(F("Modified AT instruction set available.\n\nType AT? for help"));
        if (resetDetector.getSafeMode()) {
            String str = FSPGM(safe_mode_enabled);
            Logger_notice(str);
            MySerial.println(str);
        }
    }
#endif
}

int  config_apply_wifi_settings() {

    bool station_mode_success = false;
    bool ap_mode_success = false;

    Logger_notice(F("Applying WiFi settings"));
    // config_set_blink(BLINK_FAST);

    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);

    auto flags = config._H_GET(Config().flags);

    if (flags.wifiMode & WIFI_STA) {
        WiFi.enableSTA(true);
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        WiFi.hostname(config._H_STR(Config().device_name));

        bool result;
        if (flags.stationModeDHCPEnabled) {
            result = WiFi.config((uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);
        } else {
            result = WiFi.config(config._H_GET_IP(Config().local_ip), config._H_GET_IP(Config().gateway), config._H_GET_IP(Config().subnet), config._H_GET_IP(Config().dns1), config._H_GET_IP(Config().dns2));
        }

        if (!result) {
            Logger_error(F("Failed to configure Station Mode with %s"), flags.stationModeDHCPEnabled ? String(F("DHCP")).c_str() : String(F("static address")).c_str());
        } else {
            if (!WiFi.begin(config._H_STR(Config().wifi_ssid), config._H_STR(Config().wifi_pass))) {
                Logger_error(F("Failed to start Station Mode"));
            } else {
                debug_printf_P(PSTR("Station Mode SSID %s\n"), WiFi.SSID().c_str());
                station_mode_success = true;
            }
        }
    }

    if (flags.wifiMode & WIFI_AP_STA) {

        WiFi.enableAP(true);
        if (config.get<uint8_t>(_H(Config().soft_ap.encryption)) == -1) {
            Logger_error(F("Encryption for AP mode could not be set"));
        } else {
            if (!WiFi.softAPConfig(config._H_GET_IP(Config().soft_ap.address), config._H_GET_IP(Config().soft_ap.gateway), config._H_GET_IP(Config().soft_ap.subnet))) {
                Logger_error(F("Cannot configure AP mode"));
            } else {
                if (!WiFi.softAP(config._H_STR(Config().soft_ap.wifi_ssid), config._H_STR(Config().soft_ap.wifi_pass), config._H_GET(Config().soft_ap.channel), flags.hiddenSSID)) {
                    Logger_error(F("Cannot start AP mode"));
                } else {
                    struct dhcps_lease dhcp_lease;
                    wifi_softap_dhcps_stop();
                    dhcp_lease.start_ip.addr = config._H_GET_IP(Config().soft_ap.dhcp_start);
                    dhcp_lease.end_ip.addr = config._H_GET_IP(Config().soft_ap.dhcp_end);
                    dhcp_lease.enable = flags.stationModeDHCPEnabled;
                    if (!wifi_softap_set_dhcps_lease(&dhcp_lease)) {
                        Logger_error(F("Failed to configure DHCP server"));
                    } else {
                        if (flags.stationModeDHCPEnabled) {
                            if (!wifi_softap_dhcps_start()) {
                                Logger_error(F("Failed to start DHCP server"));
                            } else {
                                ap_mode_success = true;
                                Logger_notice(F("AP Mode sucessfully initialized"));
                            }
                        } else {
                            Logger_notice(F("DHCP server disabled"));
                        }
                    }
                }
            }
        }

    }

    if (station_mode_success || ap_mode_success) {
        config_set_blink(BLINK_FAST);
    }
    if (!station_mode_success) {
        WiFi.enableSTA(false);
        WiFi.disconnect(false);
    }
    if (!ap_mode_success) {
        WiFi.enableAP(false);
        WiFi.softAPdisconnect(false);
    }

    return (station_mode_success || ap_mode_success);
}

void  config_restart() {
    system_stats_update();
    Logger_notice(F("System restart"));
    config_set_blink(BLINK_FLICKER);
    ESP.restart();
}

uint8_t WiFi_mode_connected(uint8_t mode, uint32_t *station_ip, uint32_t *ap_ip) {
    struct ip_info ip_info;
    uint8_t connected_mode = 0;

    auto flags = config.get<ConfigFlags>(_H(Config().flags));

    if (flags.wifiMode & mode) { // is any modes active?
        if ((mode & WIFI_STA) && flags.wifiMode & WIFI_STA && wifi_station_get_connect_status() == STATION_GOT_IP) { // station connected?
            if (wifi_get_ip_info(STATION_IF, &ip_info) && ip_info.ip.addr) { // verify that is has a valid IP address
                connected_mode |= WIFI_STA;
                if (station_ip) {
                    *station_ip = ip_info.ip.addr;
                }
            }
        }
        if ((mode & WIFI_AP) && flags.wifiMode & WIFI_AP) { // AP mode active?
            if (wifi_get_ip_info(SOFTAP_IF, &ip_info) && ip_info.ip.addr) { // verify that is has a valid IP address
                connected_mode |= WIFI_AP;
                if (ap_ip) {
                    *ap_ip = ip_info.ip.addr;
                }
            }
        }
    }
    return connected_mode;
}

KFCFWConfiguration::KFCFWConfiguration() : Configuration(CONFIG_EEPROM_OFFSET, EEPROM.length() - CONFIG_EEPROM_OFFSET) {
    _garbageCollectionCycleDelay = 5000;
    LoopFunctions::add([this]() {
        this->garbageCollector();
    }, reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
}

KFCFWConfiguration::~KFCFWConfiguration() {
    LoopFunctions::remove(reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
}

void KFCFWConfiguration::restoreFactorySettings() {
    debug_printf_P(PSTR("restoreFactorySettings()\n"));
    PrintString str;

    clear();
    _H_SET(Config().version, FIRMWARE_VERSION);

    ConfigFlags flags;
    memset(&flags, 0, sizeof(flags));
    flags.wifiMode = WIFI_AP;
    flags.isFactorySettings = true;
    flags.isDefaultPassword = true;
    flags.atModeEnabled = true;
    flags.softAPDHCPDEnabled = true;
    flags.stationModeDHCPEnabled = true;
    flags.ntpClientEnabled = true;
    flags.webServerMode = HTTP_MODE_UNSECURE;
    flags.webServerPerformanceModeEnabled = true;
    flags.mqttMode = MQTT_MODE_SECURE;
    flags.mqttAutoDiscoveryEnabled = true;
    flags.ledMode = MODE_SINGLE_LED;
    flags.hueEnabled = true;
    _H_SET(Config().flags, flags);

    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);
    str.printf_P(PSTR("KFC%02X%02X%02X"), mac[3], mac[4], mac[5]);
    _H_SET_STR(Config().device_name, str);
    _H_SET_STR(Config().device_pass, F("12345678"));

    _H_SET_STR(Config().soft_ap.wifi_ssid, str);
    _H_SET_STR(Config().wifi_ssid, str);

    _H_SET_IP(Config().dns1, IPAddress(8, 8, 8, 8));
    _H_SET_IP(Config().dns2, IPAddress(8, 8, 4, 4));
    _H_SET_IP(Config().local_ip, IPAddress(192, 168, 4, mac[5] <= 1 || mac[5] >= 253 ? (mac[4] <= 1 || mac[4] >= 253 ? (mac[3] <= 1 || mac[3] >= 253 ? mac[3] : rand() % 98 + 1) : mac[4]) : mac[5]));
    _H_SET_IP(Config().gateway, IPAddress(192, 168, 4, 1));
    _H_SET_IP(Config().soft_ap.address, IPAddress(192, 168, 4, 1));
    _H_SET_IP(Config().soft_ap.subnet, IPAddress(255, 255, 255, 0));
    _H_SET_IP(Config().soft_ap.gateway, IPAddress(192, 168, 4, 1));
    _H_SET_IP(Config().soft_ap.dhcp_start, IPAddress(192, 168, 4, 2));
    _H_SET_IP(Config().soft_ap.dhcp_end, IPAddress(192, 168, 4, 100));
    _H_SET(Config().soft_ap.encryption, ENC_TYPE_CCMP);
    _H_SET(Config().soft_ap.channel, 7);

#if WEBSERVER_TLS_SUPPORT
    _H_SET(Config().http_port, flags.webServerMode == HTTP_MODE_SECURE ? 443 : 80);
#elif WEBSERVER_SUPPORT
    _H_SET(Config().http_port, 80);
#endif
#if NTP_CLIENT
    _H_SET_STR(Config().ntp.timezone, F("UTC"));
    _H_SET_STR(Config().ntp.servers[0], F("pool.ntp.org"));
    _H_SET_STR(Config().ntp.servers[1], F("time.nist.gov"));
    _H_SET_STR(Config().ntp.servers[2], F("time.windows.com"));
#if USE_REMOTE_TIMEZONE
    // https://timezonedb.com/register
    _H_SET_STR(Config().ntp.remote_tz_dst_ofs_url, F("http://api.timezonedb.com/v2.1/get-time-zone?key=_YOUR_API_KEY_&by=zone&format=json&zone=${timezone}"));
#endif
#endif
    _H_SET(Config().led_pin, LED_BUILTIN);
#if MQTT_SUPPORT
    _H_SET(Config().mqtt_keepalive, 15);
    _H_SET_STR(Config().mqtt_topic, F("home/${device_name}"));
#  if ASYNC_TCP_SSL_ENABLED
    _H_SET(Config().mqtt_port, flags. flags.mqttMode == MQTT_MODE_SECURE ? 8883 : 1883);
#  else
    _H_SET(Config().mqtt_port, 1883);
#  endif
#  if MQTT_AUTO_DISCOVERY
    _H_SET_STR(Config().mqtt_discovery_prefix, F("homeassistant"));
#  endif
#endif
#if SYSLOG
    set<uint16_t>(_H(Config().syslog_port), 514);
#endif
#if HOME_ASSISTANT_INTEGRATION
    _H_SET_STR(Config().homeassistant.api_endpoint, F("http://<CHANGE_ME>:8123/api/"));
#endif
#if HUE_EMULATION
    set<uint16_t>(_H(Config().hue.tcp_port), HUE_BASE_PORT);
    _H_SET_STR(Config().hue.devices), F("lamp 1\nlamp 2"));
#endif
#if PING_MONITOR
// #error causes an execption somewhere in here
    _H_SET_STR(Config().ping.host1, F("${gateway}"));
    _H_SET_STR(Config().ping.host2, F("8.8.8.8"));
    _H_SET_STR(Config().ping.host3, F("www.google.com"));
    _H_SET(Config().ping.count, 4);
    _H_SET(Config().ping.interval, 60);
    _H_SET(Config().ping.timeout, 2000);
#endif
#if SERIAL2TCP
    _H_SET(Config().serial2tcp.port, 2323);
    _H_SET(Config().serial2tcp.auth_mode, true);
    _H_SET(Config().serial2tcp.auto_connect, false);
    _H_SET(Config().serial2tcp.auto_reconnect, 15);
    _H_SET(Config().serial2tcp.port, 2323);
    _H_SET(Config().serial2tcp.serial_port, SERIAL2TCP_HARDWARE_SERIAL);
    _H_SET(Config().serial2tcp.rx_pin, D7);
    _H_SET(Config().serial2tcp.tx_pin, D8);
    _H_SET(Config().serial2tcp.baud_rate, 115200);
    _H_SET(Config().serial2tcp.idle_timeout, 300);
    _H_SET(Config().serial2tcp.keep_alive, 60);
#endif


#if CUSTOM_CONFIG_PRESET
    #include "retracted/custom_config.h"
#endif

}

void KFCFWConfiguration::setLastError(const String &error) {
    _lastError = error;
}

const char *KFCFWConfiguration::getLastError() const {
    return _lastError.c_str();
}

uint8_t KFCFWConfiguration::getMaxChannels() {
    wifi_country_t country;
    if (!wifi_get_country(&country)) {
        country.nchan = 255;
    }
    return country.nchan;
}


void KFCFWConfiguration::garbageCollector() {
    if (_readAccess && millis() > _readAccess + _garbageCollectionCycleDelay) {
        debug_println(F("KFCFWConfiguration::garbageCollector(): releasing memory"));
        _readAccess = 0;
        release();
    }
}

void KFCFWConfiguration::setConfigDirty(bool dirty) {
    _dirty = dirty;
}

bool KFCFWConfiguration::isConfigDirty() const {
    return _dirty;
}


const String KFCFWConfiguration::getFirmwareVersion() {
    return F(FIRMWARE_VERSION_STR " Build " __BUILD_NUMBER " " __DATE__ __DEBUG_CFS_APPEND);
}
