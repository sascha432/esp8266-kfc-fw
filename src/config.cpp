/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EEPROM.h>
#include <KFCTimezone.h>
#include <time.h>
#include <OSTimer.h>
#include <LoopFunctions.h>
#include "progmem_data.h"
#include "fs_mapping.h"
#include "debug_helper.h"
#include "global.h"
#include "misc.h"
#include "session.h"
#include "kfc_fw_config.h"
#include "reset_detector.h"
#include "build.h"

/**
 *
 * TODO remove configuration from RAM and store it in flash ram. copy on write for changes
 * all paramters can be objects that load the data from flash and store changes as a copy until it is saved
 *
DEBUG (config.cpp:209 config_setup): Size of configuration: 1356
**/
Config config;
Configuration _Config(config);
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
        crc = _crc16_update(crc, eeprom[index]);
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

const char system_stats_filename[] PROGMEM = { "/system.stats" };

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

void  system_stats_update(bool open_eeprom) {

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
    File file = SPIFFS.open(FPSTR(system_stats_filename), file_mode_write());
    if (file) {
        file.write((uint8_t *)&_systemStats, sizeof(_systemStats));
        file.close();
    }
#endif

    system_stats_next_update = current_runtime + SYSTEM_STATS_UPDATE_INTERVAL;
}

void  system_stats_read() {
    uint16_t crc;

    bzero(&_systemStats, sizeof(_systemStats));

    EEPROM.begin(sizeof(_systemStats));
    EEPROM.get(0, _systemStats);
    EEPROM.end();
    crc = system_stats_crc();
    if (_systemStats.crc != crc) {

#if SPIFFS_SUPPORT
        File file = SPIFFS.open(FPSTR(system_stats_filename), file_mode_read());
        if (file) {
            bzero(&_systemStats, sizeof(_systemStats));
            file.read((uint8_t *)&_systemStats, sizeof(_systemStats));
            file.close();
            crc = system_stats_crc();
        }
#endif

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

class BlinkLEDTimer : public OSTimer {
public:
    BlinkLEDTimer() : OSTimer() {
        _pin = -1;
    }

    virtual void run() override {
        digitalWrite(_pin, _pattern.test(_counter++ % _pattern.size()) ? HIGH : LOW);
    }

    void set(uint32_t delay, int8_t pin, dynamic_bitset &pattern) {
        _pattern = pattern;
        if (_pin != pin) {
            _pin = pin;
            pinMode(_pin, OUTPUT);
            analogWrite(pin, 0);
            digitalWrite(pin, LOW);
        }
        _counter = 0;
        startTimer(delay, true);
    }

    void detach() {
        if (_pin != -1) {
            analogWrite(_pin, 0);
            digitalWrite(_pin, LOW);
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

void config_set_blink(uint16_t delay, int8_t pin) {

    if (pin == -1) {
        debug_printf_P(PSTR("Using configured LED type %d, PIN %d, blink %d\n"), _Config.getOptions().getLedMode(), config.led_pin, delay);
        pin = (_Config.getOptions().getLedMode() != MODE_NO_LED && config.led_pin != -1) ? config.led_pin : -1;
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
                pattern.setMaxSize(36);
                pattern.setBytes((uint8_t *)"\x0a\x8e\xee\x2a\x00", 5);
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
    Logger_notice(F("Starting KFCFW %s"), config_firmware_version().c_str());
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

const char config_filename[] PROGMEM = { "/configuration.backup" };

#define CONFIG_CRC_OFS (uint8_t)(sizeof(config.crc))

uint16_t  config_crc(uint8_t *src = nullptr, size_t ofs = 0) {
    if (src != nullptr) {
        return eeprom_crc(&src[CONFIG_CRC_OFS + ofs], sizeof(config) - CONFIG_CRC_OFS);
    }
    return eeprom_crc(&((uint8_t *)&config)[CONFIG_CRC_OFS], sizeof(config) - CONFIG_CRC_OFS);
}

bool config_read(bool init, size_t ofs) {
    uint16_t crc;
    bool success = true;
    debug_printf_P(PSTR("Reading configuration, offset %u\n"), ofs);

    EEPROM.begin(sizeof(config) + CONFIG_EEPROM_OFFSET + ofs);
    EEPROM.get(CONFIG_EEPROM_OFFSET + ofs, config);
    crc = config_crc();
    EEPROM.end();
#if DEBUG
    if (config.crc != crc) {
        debug_printf_P(PSTR("config_read: CRC mismatch. %08X != %08X\n"), config.crc, crc);
    }
    if (config.ident != FIRMWARE_IDENT) {
        debug_println(F("config_read: firmware identification mismatch"));
    }
#endif
    if ((config.crc != crc) || (config.ident != FIRMWARE_IDENT)) {
        success = false;
#if SPIFFS_SUPPORT
        MySerial.println(F("EEPROM CRC mismatch, trying to read backup configuration from file system"));
        String filename = FPSTR(config_filename);
        File file = SPIFFS.open(filename, file_mode_read());
#if DEBUG
        if (!file) {
            debug_printf_P(PSTR("Failed to open %s\n"), filename.c_str());
        }
        if (file.size() != sizeof(config)) {
            debug_printf_P(PSTR("File size mismatch %d != %d\n"), file.size(), sizeof(config));
        }
#endif
        if (file && file.size() == sizeof(config)) {
            if (file.readBytes((char *)&config, sizeof(config)) == sizeof(config)) {
                crc = config_crc();
                if ((config.crc == crc) && (config.ident == FIRMWARE_IDENT)) {
                    config_write();
                }
            }
        }
#endif
        if ((config.crc != crc) || (config.ident != FIRMWARE_IDENT)) {
            MySerial.println(F("Failed to load EEPROM settings, using factory defaults"));
            _Config.restoreFactorySettings();
        }
    }
    if (FIRMWARE_VERSION > config.version) {
        debug_printf_P(PSTR("Upgrading EEPROM settings from %d.%d.%d to " FIRMWARE_VERSION_STR "\n"), (config.version >> 16), (config.version >> 8) & 0xff, (config.version & 0xff));
        config_write();
    }
    return success;
}

void config_write(bool factory_reset, size_t ofs) {

    debug_printf_P(PSTR("Writing configuration, offset %u, factory reset %d\n"), ofs, factory_reset);

    if (factory_reset) {
        _Config.restoreFactorySettings();
    } else {
        config.flags &= ~FLAGS_FACTORY_SETTINGS;
    }
    config.crc = config_crc();
    EEPROM.begin(sizeof(config) + CONFIG_EEPROM_OFFSET + ofs);
    EEPROM.put(CONFIG_EEPROM_OFFSET + ofs, config);
    debug_printf_P(PSTR("CRC %08X, offset %d\n"), (unsigned int)config.crc, ofs);

#if SYSTEM_STATS
    if (ofs == 0) {
        system_stats_update(false);
    }
#endif

#if DEBUG
    EEPROM.commit();
    debug_printf_P(PSTR("reread CRC %08X %08X, offset %u\n"), config.crc, config_crc(EEPROM.getDataPtr(), CONFIG_EEPROM_OFFSET + ofs), ofs);
#endif
    EEPROM.end();

    if (!factory_reset && ofs == 0) {
        File file = SPIFFS.open(FPSTR(config_filename), file_mode_write());
        if (file) {
            file.write((const uint8_t *)&config, sizeof(config));
            file.close();
        }
    }
}

#if DEBUG

typedef struct {
    PGM_P name;
    ConfigFlags_t value;
} config_flags_to_str_t;

void  config_dump(Stream &stream) {
    static config_flags_to_str_t _flags[] = {
        { PSTR("FLAGS_FACTORY_SETTINGS"),       0x00000001 },
        { PSTR("FLAGS_MODE_STATION"),           0x00000002 },
        { PSTR("FLAGS_MODE_AP"),                0x00000004 },
        { PSTR("FLAGS_AT_MODE"),                0x00000008 },
        { PSTR("FLAGS_SOFTAP_DHCPD"),           0x00000010 },
        { PSTR("FLAGS_STATION_DHCP"),           0x00000020 },
        { PSTR("FLAGS_MQTT_ENABLED"),           0x00000040 },
        { PSTR("FLAGS_NTP_ENABLED"),            0x00000080 },
        { PSTR("FLAGS_SYSLOG_UDP"),             0x00000200 },
        { PSTR("FLAGS_SYSLOG_TCP"),             0x00000400 },
        { PSTR("FLAGS_SYSLOG_TCP_TLS"),         0x00000600 },
        { PSTR("FLAGS_HTTP_ENABLED"),           0x00001000 },
        { PSTR("FLAGS_HTTP_TLS"),               0x00002000 },
        { PSTR("FLAGS_REST_API_ENABLED"),       0x00004000 },
        { PSTR("FLAGS_SECURE_MQTT"),            0x00008000 },
        { PSTR("FLAGS_LED_SINGLE"),             0x00010000 },
        { PSTR("FLAGS_LED_TWO"),                0x00020000 },
        { PSTR("FLAGS_LED_RGB"),                0x00030000 },
        { PSTR("FLAGS_LED_MODES"),              0x00030000 },
        { PSTR("FLAGS_SERIAL2TCP_CLIENT"),      0x00040000 },
        { PSTR("FLAGS_SERIAL2TCP_SERVER"),      0x00080000 },
        { PSTR("FLAGS_SERIAL2TCP_TLS"),         0x00100000 },
        { PSTR("FLAGS_HUE_ENABLED"),            0x00200000 },
        { PSTR("FLAGS_MQTT_AUTO_DISCOVERY"),    0x00400000 },
        { PSTR("FLAGS_HIDDEN_SSID"),            0x00800000 },
        { PSTR("FLAGS_HTTP_PERF_160"),          0x01000000 },

        { NULL , 0 }
    };

    debug_port_println(stream, F("--- config ---"));
    debug_port_print_hex(stream, config.crc);
    debug_port_print_hex(stream, config.ident);
    debug_port_print_hex(stream, config.version);
    debug_port_print_char_ptr(stream, config.device_name);
    debug_port_print_char_ptr(stream, config.device_pass);
    debug_port_print_char_ptr(stream, config.wifi_ssid);
    debug_port_print_char_ptr(stream, config.wifi_pass);
    String buf;
    for (int i = 0; _flags[i].value; i++) {
        if (config.flags & _flags[i].value) {
            buf += String(FSPGM(comma_));
            buf += String(FPSTR(_flags[i].name));
        }
    }
    // for (int i = 0; _flags[i].value; i++) {
    //     if (config.flags2 & _flags[i].value) {
    //         buf += String(FSPGM(comma_));
    //         buf += String(FPSTR(_flags[i].name));
    //     }
    // }
    debug_port_print_hex(stream,  config.flags);
    // debug_port_print_hex(stream, config.flags2);
    debug_port_printf_P(stream, PSTR("flags=%s\n"), buf.substring(2).c_str());
    debug_port_print_IPAddress(stream, config.dns1);
    debug_port_print_IPAddress(stream, config.dns2);
    debug_port_print_IPAddress(stream, config.local_ip);
    debug_port_print_IPAddress(stream, config.subnet);
    debug_port_print_IPAddress(stream, config.gateway);
    debug_port_print_char_ptr(stream, config.soft_ap.wifi_ssid);
    debug_port_print_char_ptr(stream, config.soft_ap.wifi_pass);
    debug_port_print_IPAddress(stream, config.soft_ap.address);
    debug_port_print_IPAddress(stream, config.soft_ap.subnet);
    debug_port_print_IPAddress(stream, config.soft_ap.gateway);
    debug_port_print_IPAddress(stream, config.soft_ap.dhcp_start);
    debug_port_print_IPAddress(stream, config.soft_ap.dhcp_end);
    debug_port_print_int(stream, config.soft_ap.channel);
    debug_port_print_int(stream, config.soft_ap.encryption);
#if WEBSERVER_SUPPORT
    debug_port_print_int(stream, config.http_port);
#endif
#if ASYNC_TCP_SSL_ENABLED
    debug_port_print_char_ptr(stream, config.cert_passphrase);
#endif
#if NTP_CLIENT
    debug_port_print_char_ptr(stream, config.ntp.timezone);
    debug_port_print_char_ptr(stream, config.ntp.servers[0]);
    debug_port_print_char_ptr(stream, config.ntp.servers[1]);
    debug_port_print_char_ptr(stream, config.ntp.servers[2]);
    debug_port_print_char_ptr(stream, config.ntp.remote_tz_dst_ofs_url);
#endif
    debug_port_print_int(stream, config.led_pin);
#if SYSLOG
    debug_port_print_char_ptr(stream, config.syslog_host);
    debug_port_print_int(stream, config.syslog_port);
#endif
#if MQTT_SUPPORT
    debug_port_print_char_ptr(stream, config.mqtt_host);
    debug_port_print_int(stream, config.mqtt_port);
    debug_port_print_char_ptr(stream, config.mqtt_username);
    debug_port_print_char_ptr(stream, config.mqtt_password);
    debug_port_print_char_ptr(stream, config.mqtt_topic);
    debug_port_print_int(stream, config.mqtt_keepalive);
    debug_port_print_int(stream, config.mqtt_qos);
    String hexStr;
    bin2hex_append(hexStr, config.mqtt_fingerprint, sizeof(config.mqtt_fingerprint));
    debug_port_printf_P(stream, PSTR("config.mqtt_fingerprint=%s\n"), hexStr.c_str());
#  if MQTT_AUTO_DISCOVERY
    debug_port_print_char_ptr(stream, config.mqtt_discovery_prefix);
#  endif
#endif
#if HUE_EMULATION
    debug_port_print_char_ptr(stream, config.hue.devices);
    debug_port_print_int(stream, config.hue.tcp_port);
#endif
#if SERIAL2TCP
    debug_port_print_char_ptr(stream, config.serial2tcp.host);
    debug_port_print_int(stream, config.serial2tcp.port);
    debug_port_print_int(stream, config.serial2tcp.auth_mode);
    debug_port_print_char_ptr(stream, config.serial2tcp.username);
    debug_port_print_char_ptr(stream, config.serial2tcp.password);
    debug_port_print_int(stream, config.serial2tcp.auto_connect);
    debug_port_print_int(stream, config.serial2tcp.auto_reconnect);
    debug_port_print_int(stream, config.serial2tcp.baud_rate);
    debug_port_print_int(stream, config.serial2tcp.serial_port);
    debug_port_print_int(stream, config.serial2tcp.rx_pin);
    debug_port_print_int(stream, config.serial2tcp.tx_pin);
#endif
    debug_port_println(stream, F("---"));
}
#endif

const String config_firmware_version() {
    return F(FIRMWARE_VERSION_STR " Build " __BUILD_NUMBER " " __DATE__ __DEBUG_CFS_APPEND);
}

void config_version() {
    MySerial.printf_P(PSTR("KFC Firmware %s\nFlash size %s\n"), config_firmware_version().c_str(), formatBytes(ESP.getFlashChipRealSize()).c_str());
}

void  config_info() {
    config_version();
    if (config.flags & FLAGS_MODE_AP) {
        MySerial.printf_P(PSTR("AP Mode SSID %s\n"), config.soft_ap.wifi_ssid);
    } else if (config.flags & FLAGS_MODE_STATION) {
        MySerial.printf_P(PSTR("Station Mode SSID %s\n"), config.wifi_ssid);
    }
    if (config.flags & FLAGS_FACTORY_SETTINGS) {
        MySerial.println(F("Running on factory settings."));
    }
    MySerial.printf_P(PSTR("Device %s ready!\n"), config.device_name);
#if AT_MODE_SUPPORTED
    if (config.flags & FLAGS_AT_MODE) {
        MySerial.println(F("Modified AT instruction set available.\n\nType AT? for help"));
        if (resetDetector.getSafeMode()) {
            MySerial.print(F("SAFE MODE> "));
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

    if (config.flags & FLAGS_MODE_STATION) {
        WiFi.enableSTA(true);
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        WiFi.hostname(config.device_name);

        uint32_t local_ip = 0;
        uint32_t gateway = 0;
        uint32_t subnet = 0;
        uint32_t dns1 = 0;
        uint32_t dns2 = 0;

        if (!_Config.getOptions().isStationDhcp()) {

            local_ip = static_cast<uint32_t>(config.local_ip);
            gateway = static_cast<uint32_t>(config.gateway);
            subnet = static_cast<uint32_t>(config.subnet);
            dns1 = static_cast<uint32_t>(config.dns1);
            dns2 = static_cast<uint32_t>(config.dns2);
        }

        if (!WiFi.config(local_ip, gateway, subnet, dns1, dns2)) {
            Logger_error(F("Failed to configure Station Mode with %s"), _Config.getOptions().isStationDhcp() ? str_P(F("DHCP")) : str_P(F("static address")));
        } else {
            if (!WiFi.begin(config.wifi_ssid, config.wifi_pass)) {
                Logger_error(F("Failed to start Station Mode"));
            } else {
                debug_printf_P(PSTR("Station Mode SSID %s\n"), WiFi.SSID().c_str());
                station_mode_success = true;
            }
        }
    }

    if (config.flags & FLAGS_MODE_AP) {

        WiFi.enableAP(true);
        if (WiFi.encryptionType(config.soft_ap.encryption) == -1) {
            Logger_error(F("Encryption for AP mode could not be set"));
        } else {
            if (!WiFi.softAPConfig(config.soft_ap.address, config.soft_ap.gateway, config.soft_ap.subnet)) {
                Logger_error(F("Cannot configure AP mode"));
            } else {
                if (!WiFi.softAP(config.soft_ap.wifi_ssid, config.soft_ap.wifi_pass, config.soft_ap.channel, _Config.getOptions().isHiddenSSID())) {
                    Logger_error(F("Cannot start AP mode"));
                } else {
                    struct dhcps_lease dhcp_lease;
                    wifi_softap_dhcps_stop();
                    dhcp_lease.start_ip.addr = static_cast<uint32_t>(config.soft_ap.dhcp_start);
                    dhcp_lease.end_ip.addr = static_cast<uint32_t>(config.soft_ap.dhcp_end);
                    dhcp_lease.enable = (config.flags & FLAGS_SOFTAP_DHCPD);
                    if (!wifi_softap_set_dhcps_lease(&dhcp_lease)) {
                        Logger_error(F("Failed to configure DHCP server"));
                    } else {
                        if (_Config.getOptions().isSoftApDhcpd()) {
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

Configuration::Configuration(Config &config) : _options(config), _config(config) {
    _dirty = false;
}

const char  *Configuration::getLastError() {
    return last_error.c_str();
}

#if SYSLOG

SyslogProtocol ConfigOptions::getSyslogProtocol() {
    if (_config.flags & FLAGS_SYSLOG_UDP) {
        return SYSLOG_PROTOCOL_UDP;
    } else if (_config.flags & FLAGS_SYSLOG_TCP) {
#  if ASYNC_TCP_SSL_ENABLED
        if (_config.flags & FLAGS_SYSLOG_TCP_TLS) {
            return SYSLOG_PROTOCOL_TCP_TLS;
        }
#  endif
        return SYSLOG_PROTOCOL_TCP;
    }
    return SYSLOG_PROTOCOL_NONE;
}

bool  ConfigOptions::isSyslogProtocol(SyslogProtocol protocol) {
    return (getSyslogProtocol() == protocol);
}

void  ConfigOptions::setSyslogProtocol(SyslogProtocol protocol) {
    _config.flags &= ~FLAGS_SYSLOG_MASK;
    switch (protocol) {
        case SYSLOG_PROTOCOL_UDP:
            _config.flags |= FLAGS_SYSLOG_UDP;
            break;
        case SYSLOG_PROTOCOL_TCP:
            _config.flags |= FLAGS_SYSLOG_TCP;
            break;
#  if ASYNC_TCP_SSL_ENABLED
        case SYSLOG_PROTOCOL_TCP_TLS:
            _config.flags |= FLAGS_SYSLOG_TCP_TLS;
            break;
#  else
        case SYSLOG_PROTOCOL_TCP_TLS:
#  endif
        case SYSLOG_PROTOCOL_FILE:
        case SYSLOG_PROTOCOL_NONE:
            break;
    }
}
#endif

void ConfigOptions::_setFlags(uint32_t bitset, bool enabled, uint32_t mask) {
    _config.flags &= ~mask;
    if (enabled) {
        _config.flags |= bitset;
    } else {
        _config.flags &= ~bitset;
    }
}

void  ConfigOptions::_setFlags(ConfigFlags_t bitset, bool enabled) {
    if (enabled) {
        _config.flags |= bitset;
    } else {
        _config.flags &= ~bitset;
    }
}

uint32_t ConfigOptions::_getFlags(ConfigFlags_t bitset) {
    return (_config.flags & bitset);
}

bool ConfigOptions::_isFlags(ConfigFlags_t bitset) {
    return (_config.flags & bitset) == bitset;
}

void  Configuration::restoreFactorySettings() {
    debug_printf_P(PSTR("restoreFactorySettings()\n"));

    memset(&_config, 0, sizeof(_config));
    _config.ident = FIRMWARE_IDENT;
    _config.version = FIRMWARE_VERSION;
    _config.flags = FLAGS_MODE_AP | FLAGS_FACTORY_SETTINGS | FLAGS_AT_MODE | FLAGS_SOFTAP_DHCPD | FLAGS_STATION_DHCP | FLAGS_NTP_ENABLED | FLAGS_HTTP_ENABLED | FLAGS_REST_API_ENABLED | FLAGS_SECURE_MQTT | FLAGS_LED_SINGLE | FLAGS_HUE_ENABLED | FLAGS_HTTP_PERF_160 | FLAGS_MQTT_AUTO_DISCOVERY;
    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);
    snprintf_P(_config.device_name, sizeof(_config.device_name), PSTR("KFC%02X%02X%02X"), mac[3], mac[4], mac[5]);
    strcpy(_config.soft_ap.wifi_ssid, _config.device_name);
    strcpy(_config.wifi_ssid, _config.device_name);
    strcpy_P(_config.device_pass, PSTR("12345678"));
    strcpy(_config.soft_ap.wifi_pass, _config.device_pass);
    strcpy(_config.soft_ap.wifi_pass, _config.device_pass);
    _config.dns1 = IPAddress(8, 8, 8, 8);
    _config.dns2 = IPAddress(8, 8, 4, 4);
    _config.local_ip = IPAddress(192, 168, 4, mac[5] <= 1 || mac[5] >= 253 ? (mac[4] <= 1 || mac[4] >= 253 ? (mac[3] <= 1 || mac[3] >= 253 ? mac[3] : rand() % 98 + 1) : mac[4]) : mac[5]);
    _config.subnet = IPAddress(255, 255, 255, 0);
    _config.gateway = IPAddress(192, 168, 4, 1);
    _config.soft_ap.address = IPAddress(192, 168, 4, 1);
    _config.soft_ap.subnet = IPAddress(255, 255, 255, 0);
    _config.soft_ap.gateway = IPAddress(192, 168, 4, 1);
    _config.soft_ap.dhcp_start = IPAddress(192, 168, 4, 2);
    _config.soft_ap.dhcp_end = IPAddress(192, 168, 4, 100);
    _config.soft_ap.encryption = ENC_TYPE_CCMP;
    _config.soft_ap.channel = 7;
#if WEBSERVER_TLS_SUPPORT
    _config.http_port = _Config.getOptions().isHttpServerTLS() ? 443 : 80;
#elif WEBSERVER_SUPPORT
    _config.http_port = 80;
#endif
#if NTP_CLIENT
    strcpy_P(_config.ntp.timezone, PSTR("UTC"));
    strcpy_P(_config.ntp.servers[0], PSTR("pool.ntp.org"));
    strcpy_P(_config.ntp.servers[1], PSTR("time.nist.gov"));
    strcpy_P(_config.ntp.servers[2], PSTR("time.windows.com"));
#if USE_REMOTE_TIMEZONE
    // https://timezonedb.com/register
    strncpy_P(_config.ntp.remote_tz_dst_ofs_url, PSTR("http://api.timezonedb.com/v2.1/get-time-zone?key=_YOUR_API_KEY_&by=zone&format=json&zone=${timezone}"), sizeof(_config.ntp.remote_tz_dst_ofs_url));
#endif
#endif
    _config.led_pin = 2;
#if MQTT_SUPPORT
    config.mqtt_keepalive = 15;
    strcpy_P(_config.mqtt_topic, PSTR("home/${device_name}"));
#  if ASYNC_TCP_SSL_ENABLED
    _config.mqtt_port = 8883;
#  else
    _config.mqtt_port = 1883;
#  endif
#  if MQTT_AUTO_DISCOVERY
    strcpy_P(_config.mqtt_discovery_prefix, PSTR("homeassistant"));
#  endif
#endif
#if SYSLOG
    _config.syslog_port = 514;
#endif
#if HOME_ASSISTANT_INTEGRATION
    strcpy_P(_config.homeassistant.api_endpoint, PSTR("http://ip_address:8123/api/"));
#endif
#if HUE_EMULATION
    _config.hue.tcp_port = HUE_BASE_PORT;
    strcpy_P(_config.hue.devices, PSTR("lamp 1\nlamp 2"));
#endif
#if PING_MONITOR
// #error causes an execption somewhere in here
    strncpy(_config.ping.host1, to_c_str(_config.gateway), sizeof(_config.ping.host1));
    strcpy_P(_config.ping.host2, PSTR("8.8.8.8"));
    strcpy_P(_config.ping.host3, PSTR("www.google.com"));
    _config.ping.interval = 60;
    _config.ping.count = 4;
    _config.ping.timeout = 2000;
#endif
#if SERIAL2TCP
    _config.serial2tcp.port = 2323;
    _config.serial2tcp.auth_mode = true;
    _config.serial2tcp.auto_connect = false;
    _config.serial2tcp.auto_reconnect = 15;
    _config.serial2tcp.port = 2323;
    _config.serial2tcp.serial_port = SERIAL2TCP_HARDWARE_SERIAL;
    _config.serial2tcp.rx_pin = D7;
    _config.serial2tcp.tx_pin = D8;
    _config.serial2tcp.baud_rate = 115200;
    _config.serial2tcp.idle_timeout = 300;
    _config.serial2tcp.keep_alive = 60;
#endif


#if CUSTOM_CONFIG_PRESET
    #include "retracted/custom_config.h"
#endif
    debug_printf_P(PSTR("restoreFactorySettings() return\n"));
}

void  Configuration::setDirty(bool dirty) {
    _dirty = dirty;
}

bool  Configuration::isDirty() {
    return _dirty;
}

uint8_t WiFi_mode_connected(uint8_t mode, uint32_t *station_ip, uint32_t *ap_ip) {
    struct ip_info ip_info;
    uint8_t connected_mode = 0;

    if (_Config.getOptions().getWiFiMode() & mode) { // is any modes active?
        if ((mode & WIFI_STA) && _Config.getOptions().isWiFiStationMode() && wifi_station_get_connect_status() == STATION_GOT_IP) { // station connected?
            if (wifi_get_ip_info(STATION_IF, &ip_info) && ip_info.ip.addr) { // verify that is has a valid IP address
                connected_mode |= WIFI_STA;
                if (station_ip) {
                    *station_ip = ip_info.ip.addr;
                }
            }
        }
        if ((mode & WIFI_AP) && _Config.getOptions().isWiFiSoftApMode()) { // AP mode active?
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
