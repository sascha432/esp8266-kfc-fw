/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <session.h>
#include <buffer.h>
#include <crc16.h>
#include "kfc_fw_config_types.h"

#include <push_pack.h>

#if 0
#define __CDBG_printf(...)                                                  __DBG_printf(__VA_ARGS__)
#define __CDBG_dump(class_name, cfg)                                        cfg.dump<class_name>();
#define __CDBG_dumpString(name)                                             DEBUG_OUTPUT.printf_P(PSTR("%s [%04X]=%s\n"), _STRINGIFY(name), k##name##ConfigHandle, get##name());
#define CONFIG_DUMP_STRUCT_INFO(type)                                       DEBUG_OUTPUT.printf_P(PSTR("--- config struct handle=%04x\n"), type::kConfigStructHandle);
#define CONFIG_DUMP_STRUCT_VAR(name)                                        { DEBUG_OUTPUT.print(_STRINGIFY(name)); DEBUG_OUTPUT.print("="); DEBUG_OUTPUT.println(this->name); }
#else
#define __CDBG_printf(...)
#define __CDBG_dump(class_name, cfg)
#define __CDBG_dumpString(name)
#define CONFIG_DUMP_STRUCT_INFO(type)
#define CONFIG_DUMP_STRUCT_VAR(name)
#endif

#undef DEFAULT

#define CREATE_ZERO_CONF_DEFAULT(service, proto, variable, default_value)   "${zeroconf:" service "." proto "," variable "|" default_value "}"
#define CREATE_ZERO_CONF_NO_DEFAULT(service, proto, variable)               "${zeroconf:" service "." proto "," variable "}"


#ifndef _H
#define _H_DEFINED_KFCCONFIGURATIONCLASSES                                  1
#define _H(name)                                                            constexpr_crc16_update(_STRINGIFY(name), constexpr_strlen(_STRINGIFY(name)))
#define CONFIG_GET_HANDLE_STR(name)                                         constexpr_crc16_update(name, constexpr_strlen(name))
#endif

#define CREATE_STRING_GETTER_SETTER_BINARY(class_name, name, len) \
    static constexpr size_t k##name##MaxSize = len; \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static size_t get##name##Size() { return k##name##MaxSize; } \
    static const uint8_t *get##name() { return loadBinaryConfig(k##name##ConfigHandle, k##name##MaxSize); } \
    static void set##name(const uint8_t *data) { storeBinaryConfig(k##name##ConfigHandle, data, k##name##MaxSize); }

#define CREATE_STRING_GETTER_SETTER(class_name, name, len) \
    static constexpr size_t k##name##MaxSize = len; \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static const char *get##name() { return loadStringConfig(k##name##ConfigHandle); } \
    static void set##name(const char *str) { storeStringConfig(k##name##ConfigHandle, str); } \
    static void set##name(const __FlashStringHelper *str) { set##name(reinterpret_cast<PGM_P>(str)); } \
    static void set##name(const String &str) { set##name(str.c_str()); }

#define CREATE_GETTER_SETTER_IP(class_name, name) \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static const IPAddress get##name() { return IPAddress(*(uint32_t *)loadBinaryConfig(k##name##ConfigHandle, sizeof(uint32_t))); } \
    static void set##name(const IPAddress &address) { storeBinaryConfig(k##name##ConfigHandle, &static_cast<uint32_t>(address), sizeof(uint32_t)); }


#if DEBUG

extern const char *handleNameDeviceConfig_t;
extern const char *handleNameWebServerConfig_t;
extern const char *handleNameSettingsConfig_t;
extern const char *handleNameAlarm_t;
extern const char *handleNameSerial2TCPConfig_t;
extern const char *handleNameMqttConfig_t;
extern const char *handleNameSyslogConfig_t;

#define CIF_DEBUG(...) __VA_ARGS__

#else

#define CIF_DEBUG(...)

#endif

namespace KFCConfigurationClasses {


    using HandleType = uint16_t;

    const void *loadBinaryConfig(HandleType handle, uint16_t &length);
    void *loadWriteableBinaryConfig(HandleType handle, uint16_t length);
    void storeBinaryConfig(HandleType handle, const void *data, uint16_t length);
    const char *loadStringConfig(HandleType handle);
    void storeStringConfig(HandleType handle, const char *);

    template<typename ConfigType, HandleType handleArg CIF_DEBUG(, const char **handleName)>
    class ConfigGetterSetter {
    public:
        static constexpr uint16_t kConfigStructHandle = handleArg;
        using ConfigStructType = ConfigType;

        static ConfigType getConfig()
        {
            __CDBG_printf("getConfig=%04x size=%u name=%s", kConfigStructHandle, sizeof(ConfigType), *handleName);
            uint16_t length = sizeof(ConfigType);
            auto ptr = loadBinaryConfig(kConfigStructHandle, length);
            if (!ptr || length != sizeof(ConfigType)) {
                __DBG_printf("binary handle=%04x name=%s stored_size=%u mismatch. setting default values size=%u", kConfigStructHandle, *handleName, length, sizeof(ConfigType));
                ConfigType cfg = {};
                void *newPtr = loadWriteableBinaryConfig(kConfigStructHandle, sizeof(ConfigType));
                ptr = memcpy(newPtr, &cfg, sizeof(ConfigType));
            }
            return *reinterpret_cast<const ConfigType *>(ptr);
        }

        static void setConfig(const ConfigType &params)
        {
            __CDBG_printf("setConfig=%04x size=%u name=%s", kConfigStructHandle, sizeof(ConfigType), *handleName);
            storeBinaryConfig(kConfigStructHandle, &params, sizeof(params));
        }

        static ConfigType &getWriteableConfig()
        {
            __CDBG_printf("getWriteableConfig=%04x name=%s size=%u", kConfigStructHandle, *handleName, sizeof(ConfigType));
            return *reinterpret_cast<ConfigType *>(loadWriteableBinaryConfig(kConfigStructHandle, sizeof(ConfigType)));
        }
    };

    struct System {

        // --------------------------------------------------------------------
        // Flags

        class Flags {
        public:
            Flags();
            Flags(bool load) {
                *this = load ? getFlags() : Flags();
            }
            Flags(ConfigFlags flags) : _flags(flags) {
#if 0
                __DBG_printf("wifi_mode=%u", _flags.getWifiMode());
                __DBG_printf("is_factory_settings=%u", _flags.is_factory_settings);
                __DBG_printf("is_default_password=%u", _flags.is_default_password);
                __DBG_printf("use_static_ip_during_wakeup=%u", _flags.use_static_ip_during_wakeup);
                __DBG_printf("is_at_mode_enabled=%u", _flags.is_at_mode_enabled);
                __DBG_printf("is_softap_ssid_hidden=%u", _flags.is_softap_ssid_hidden);
                __DBG_printf("is_softap_dhcpd_enabled=%u", _flags.is_softap_dhcpd_enabled);
                __DBG_printf("is_station_mode_enabled=%u", _flags.is_station_mode_enabled);
                __DBG_printf("is_softap_enabled=%u", _flags.is_softap_enabled);
                __DBG_printf("is_softap_standby_mode_enabled=%u", _flags.is_softap_standby_mode_enabled);
                __DBG_printf("is_station_mode_dhcp_enabled=%u", _flags.is_station_mode_dhcp_enabled);
                __DBG_printf("is_led_on_when_connected=%u", _flags.is_led_on_when_connected);
                __DBG_printf("is_mdns_enabled=%u", _flags.is_mdns_enabled);
                __DBG_printf("is_ntp_client_enabled=%u", _flags.is_ntp_client_enabled);
                __DBG_printf("is_syslog_enabled=%u", _flags.is_syslog_enabled);
                __DBG_printf("is_web_server_enabled=%u", _flags.is_web_server_enabled);
                __DBG_printf("is_webserver_performance_mode_enabled=%u", _flags.is_webserver_performance_mode_enabled);
                __DBG_printf("is_mqtt_enabled=%u", _flags.is_mqtt_enabled);
                __DBG_printf("is_rest_api_enabled=%u", _flags.is_rest_api_enabled);
                __DBG_printf("is_serial2tcp_enabled=%u", _flags.is_serial2tcp_enabled);
                __DBG_printf("is_webui_enabled=%u", _flags.is_webui_enabled);
                __DBG_printf("is_webalerts_enabled=%u", _flags.is_webalerts_enabled);
#endif
            }
            ConfigFlags *operator->() {
                return &_flags;
            }
            static Flags getFlags();
            static ConfigFlags get();
            static ConfigFlags &getWriteable();
            static void set(ConfigFlags flags);
            void write();

            bool isWiFiEnabled() const {
                return _flags.getWifiMode() & WIFI_AP_STA;
            }
            bool isSoftAPEnabled() const {
                return _flags.getWifiMode() & WIFI_AP;
            }
            bool isStationEnabled() const {
                return _flags.getWifiMode() & WIFI_STA;
            }
            WiFiMode getWiFiMode() const {
                return static_cast<WiFiMode>(_flags.getWifiMode());
            }
            bool isSoftApStandByModeEnabled() const {
                if (isSoftAPEnabled()) {
                    return _flags.is_softap_standby_mode_enabled;
                }
                return false;
            }
            bool isMDNSEnabled() const {
                return _flags.is_mdns_enabled;
            }
            void setMDNSEnabled(bool state) {
                _flags.is_mdns_enabled = state;
            }
            bool isWebUIEnabled() const {
                return _flags.is_webui_enabled;
            }
            bool isMQTTEnabled() const;
            void setMQTTEnabled(bool state);
            bool isSyslogEnabled() const;
            void setSyslogEnabled(bool state);

        private:
            ConfigFlags _flags;
        };

        // --------------------------------------------------------------------
        // Device

        class DeviceConfig {
        public:
            enum class StatusLEDModeType : uint8_t {
                OFF_WHEN_CONNECTED = 0,
                SOLID_WHEN_CONNECTED = 1,
            };

            typedef struct __attribute__packed__ DeviceConfig_t {
                uint32_t config_version;
                uint16_t safe_mode_reboot_timeout_minutes;
                uint16_t webui_cookie_lifetime_days;
                uint16_t zeroconf_timeout;
                union __attribute__packed__ {
                    StatusLEDModeType status_led_mode_enum;
                    uint8_t status_led_mode;
                };

                uint16_t getSafeModeRebootTimeout() const {
                    return safe_mode_reboot_timeout_minutes;
                }
                uint16_t getWebUICookieLifetime() const {
                    return webui_cookie_lifetime_days;
                }
                uint32_t getWebUICookieLifetimeInSeconds() const {
                    return webui_cookie_lifetime_days * 86400U;
                }
                StatusLEDModeType getStatusLedMode() const {
                    return status_led_mode_enum;
                }

                DeviceConfig_t() : config_version(FIRMWARE_VERSION), safe_mode_reboot_timeout_minutes(0), webui_cookie_lifetime_days(30), zeroconf_timeout(5000), status_led_mode_enum(StatusLEDModeType::SOLID_WHEN_CONNECTED) {}

            } DeviceConfig_t;
        };

        class Device : public DeviceConfig, public ConfigGetterSetter<DeviceConfig::DeviceConfig_t, _H(MainConfig().system.device.cfg) CIF_DEBUG(, &handleNameDeviceConfig_t)> {
        public:
            Device() {
            }
            static void defaults();

            CREATE_STRING_GETTER_SETTER(MainConfig().system.device, Name, 16);
            CREATE_STRING_GETTER_SETTER(MainConfig().system.device, Title, 32);

            static constexpr size_t kPasswordMinSize = 6;
            CREATE_STRING_GETTER_SETTER(MainConfig().system.device, Password, 64);
            static constexpr size_t kTokenMinSize = SESSION_DEVICE_TOKEN_MIN_LENGTH;
            CREATE_STRING_GETTER_SETTER(MainConfig().system.device, Token, 255);

            static constexpr uint16_t kZeroConfMinTimeout = 1000;
            static constexpr uint16_t kZeroConfMaxTimeout = 60000;

            static constexpr uint16_t kWebUICookieMinLifetime = 3;
            static constexpr uint16_t kWebUICookieMaxLifetime = 360;

            // static void setSafeModeRebootTime(uint16_t minutes);
            // static uint16_t getSafeModeRebootTime();
            // static void setWebUIKeepLoggedInDays(uint16_t days);
            // static uint16_t getWebUIKeepLoggedInDays();
            // static uint32_t getWebUIKeepLoggedInSeconds();
            // static void setStatusLedMode(StatusLEDModeEnum mode);
            // static StatusLEDModeEnum getStatusLedMode();

        public:
        };

        // --------------------------------------------------------------------
        // Firmware

        class Firmware {
        public:
            static void defaults();

            // sha1 of the firmware.elf file
            static const uint8_t *getElfHash(uint16_t &length);     // length getElfHashSize byte
            static uint8_t getElfHashHex(String &str);
            static void setElfHash(const uint8_t *data);            // length getElfHashSize byte
            static void setElfHashHex(const char *data);            // getElfHashHexSize() + nul byte
            static constexpr uint8_t getElfHashSize() {
                return static_cast<uint8_t>(sizeof(elf_sha1));
            }
            static constexpr uint8_t getElfHashHexSize() {
                return static_cast<uint8_t>(sizeof(elf_sha1)) * 2;
            }

            CREATE_STRING_GETTER_SETTER(MainConfig().system.firmware, PluginBlacklist, 255);

            uint8_t elf_sha1[20];
        };

        class WebServerConfig {
        public:
            enum class ModeType : uint8_t {
                MIN = 0,
                DISABLED = 0,
                UNSECURE,
                SECURE,
                MAX
            };

            typedef struct __attribute__packed__ WebServerConfig_t {
                uint16_t port;
                uint8_t is_https: 1;

                WebServerConfig_t() : port(80), is_https(false) {}

            } WebServerConfig_t;
        };

        class WebServer : public WebServerConfig, public ConfigGetterSetter<WebServerConfig::WebServerConfig_t, _H(MainConfig().system.webserver) CIF_DEBUG(, &handleNameWebServerConfig_t)> {
        public:
            static void defaults();
            static ModeType getMode();
            static void setMode(ModeType mode);
        };

        Flags flags;
        Device device;
        Firmware firmware;
        WebServer webserver;

    };

    // --------------------------------------------------------------------
    // Network

    struct Network {

        static constexpr uint32_t createIPAddress(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
            return a | (b << 8U) | (c << 16U) | (d << 24U);
        }

        // --------------------------------------------------------------------
        // Settings
        class SettingsConfig {
        public:
            typedef struct __attribute__packed__ SettingsConfig_t {
                uint32_t _localIp;
                uint32_t _subnet;
                uint32_t _gateway;
                uint32_t _dns1;
                uint32_t _dns2;

                SettingsConfig_t() :
                    _subnet(createIPAddress(255, 255, 255, 0)),
                    _gateway(createIPAddress(192, 168, 4, 1)),
                    _dns1(createIPAddress(8, 8, 8, 8)),
                    _dns2(createIPAddress(8, 8, 4, 4))
                {
                    uint8_t mac[6];
                    WiFi.macAddress(mac);
                    _localIp = IPAddress(192, 168, 4, mac[5] <= 1 || mac[5] >= 253 ? (mac[4] <= 1 || mac[4] >= 253 ? (mac[3] <= 1 || mac[3] >= 253 ? mac[3] : rand() % 98 + 1) : mac[4]) : mac[5]);
                }

            } SettingsConfig_t;
        };

        class Settings : public SettingsConfig, public ConfigGetterSetter<SettingsConfig::SettingsConfig_t, _H(MainConfig().network.settings) CIF_DEBUG(, &handleNameSettingsConfig_t)> {
        public:
            static void defaults();

            IPAddress localIp() const {
                return getConfig()._localIp;
            }
            IPAddress subnet() const {
                return getConfig()._subnet;
            }
            IPAddress gateway() const {
                return getConfig()._gateway;
            }
            IPAddress dns1() const {
                return getConfig()._dns1;
            }
            IPAddress dns2() const {
                return getConfig()._dns2;
            }
        };

        // --------------------------------------------------------------------
        // SoftAP

        class SoftAP {
        public:
            using EncryptionType = WiFiEncryptionType;

            SoftAP();
            static SoftAP read();
            static SoftAP &getWriteable();
            static SoftAP &getWriteableConfig() {
                return getWriteable();
            }
            static void defaults();
            void write();

            IPAddress address() const {
                return _address;
            }
            IPAddress subnet() const {
                return _subnet;
            }
            IPAddress gateway() const {
                return _gateway;
            }
            IPAddress dhcpStart() const {
                return _dhcpStart;
            }
            IPAddress dhcpEnd() const {
                return _dhcpEnd;
            }
            uint8_t channel() const {
                return _channel;
            }
            EncryptionType encryption() const {
                return static_cast<EncryptionType>(_encryption);
            }

            struct __attribute__packed__ {
                uint32_t _address;
                uint32_t _subnet;
                uint32_t _gateway;
                uint32_t _dhcpStart;
                uint32_t _dhcpEnd;
                uint8_t _channel;
                uint8_t _encryption;
            };
        };

        // --------------------------------------------------------------------
        // WiFi

        class WiFiConfig {
        public:
            CREATE_STRING_GETTER_SETTER(MainConfig().network.WiFiConfig, SSID, 32);
            static constexpr size_t kPasswordMinSize = 8;
            CREATE_STRING_GETTER_SETTER(MainConfig().network.WiFiConfig, Password, 32);
            CREATE_STRING_GETTER_SETTER(MainConfig().network.WiFiConfig, SoftApSSID, 32);
            static constexpr size_t kSoftApPasswordMinSize = 8;
            CREATE_STRING_GETTER_SETTER(MainConfig().network.WiFiConfig, SoftApPassword, 32);
        };

        Settings settings;
        SoftAP softAp;
        WiFiConfig WiFiConfig;
    };

    // --------------------------------------------------------------------
    // Plugins

    struct Plugins {

        // --------------------------------------------------------------------
        // Home Assistant

        class HomeAssistant {
        public:
            typedef enum : uint8_t {
                NONE = 0,
                TURN_ON,
                TURN_OFF,
                TOGGLE,
                TRIGGER,
                SET_BRIGHTNESS,     // values: brightness(0)
                CHANGE_BRIGHTNESS,  // values: step(0), min brightness(1), max brightness(2), turn off if less than min. brightness(3)
                SET_KELVIN,         // values: kelvin(0)
                SET_MIREDS,         // values: mireds(0)
                SET_RGB_COLOR,      // values: red(0), green(1), blue(2)
                MEDIA_PREVIOUS_TRACK,
                MEDIA_NEXT_TRACK,
                MEDIA_STOP,
                MEDIA_PLAY,
                MEDIA_PAUSE,
                MEDIA_PLAY_PAUSE,
                VOLUME_UP,
                VOLUME_DOWN,
                VOLUME_SET,         // values: volume(0) in %
                VOLUME_MUTE,
                VOLUME_UNMUTE,
                MQTT_SET,
                MQTT_TOGGLE,
                MQTT_INCR,
                MQTT_DECR,
                __END,
            } ActionEnum_t;
            typedef struct __attribute__packed__ {
                uint16_t id: 16;
                ActionEnum_t action;
                uint8_t valuesLen;
                uint8_t entityLen;
                uint8_t apiId;
            } ActionHeader_t;

            class Action {
            public:
                typedef std::vector<int32_t> ValuesVector;

                Action() = default;
                Action(uint16_t id, uint8_t apiId, ActionEnum_t action, const ValuesVector &values, const String &entityId) : _id(id), _apiId(apiId), _action(action), _values(values), _entityId(entityId) {
                }
                bool operator==(ActionEnum_t id) const {
                    return _id == id;
                }
                bool operator==(uint16_t id) const {
                    return _id == id;
                }
                ActionEnum_t getAction() const {
                    return _action;
                }
                const __FlashStringHelper *getActionFStr() const {
                    return HomeAssistant::getActionStr(_action);
                }
                void setAction(ActionEnum_t action) {
                    _action = action;
                }
                uint16_t getId() const {
                    return _id;
                }
                void setId(uint16_t id) {
                    _id = id;
                }
                uint8_t getApiId() const {
                    return _apiId;
                }
                void setApiId(uint8_t apiId) {
                    _apiId = apiId;
                }
                int32_t getValue(uint8_t num) const {
                    if (num < _values.size()) {
                        return _values.at(num);
                    }
                    return 0;
                }
                void setValues(const ValuesVector &values) {
                    _values = values;
                }
                ValuesVector &getValues() {
                    return _values;
                }
                String getValuesStr() const {
                    return implode(',', _values);
                }
                uint8_t getNumValues() const {
                    return _values.size();
                }
                const String &getEntityId() const {
                    return _entityId;
                }
                void setEntityId(const String &entityId) {
                    _entityId = entityId;
                }
            private:
                uint16_t _id;
                uint8_t _apiId;
                ActionEnum_t _action;
                ValuesVector _values;
                String _entityId;
            };

            typedef std::vector<Action> ActionVector;

            HomeAssistant() {
            }

            static void defaults();
            static void setApiEndpoint(const String &endpoint, uint8_t apiId = 0);
            static void setApiToken(const String &token, uint8_t apiId = 0);
            static const char *getApiEndpoint(uint8_t apiId = 0);
            static const char *getApiToken(uint8_t apiId = 0);
            static void getActions(ActionVector &actions);
            static void setActions(ActionVector &actions);
            static Action getAction(uint16_t id);
            static const __FlashStringHelper *getActionStr(ActionEnum_t action);

            char api_endpoint[128];
            char token[250];
            char api_endpoint1[128];
            char token1[250];
            char api_endpoint2[128];
            char token2[250];
            char api_endpoint3[128];
            char token3[250];
            uint8_t *actions;
        };

        // --------------------------------------------------------------------
        // Remote

        #ifndef IOT_REMOTE_CONTROL_BUTTON_COUNT
        #define IOT_REMOTE_CONTROL_BUTTON_COUNT 4
        #endif

        class RemoteControl {
        public:
            typedef struct __attribute__packed__ {
                uint16_t shortpress;
                uint16_t longpress;
                uint16_t repeat;
            } ComboAction_t;

            typedef struct __attribute__packed__ {
                uint16_t shortpress;
                uint16_t longpress;
                uint16_t repeat;
                ComboAction_t combo[IOT_REMOTE_CONTROL_BUTTON_COUNT];
            } Action_t;

            struct __attribute__packed__ {
                uint8_t autoSleepTime: 8;
                uint16_t deepSleepTime: 16;       // ESP8266 max. ~14500 seconds, 0 = indefinitely
                uint16_t longpressTime;
                uint16_t repeatTime;
                Action_t actions[IOT_REMOTE_CONTROL_BUTTON_COUNT];
            };

            RemoteControl() {
                autoSleepTime = 2;
                deepSleepTime = 0;
                longpressTime = 750;
                repeatTime = 500;
                memset(&actions, 0, sizeof(actions));
            }

            void validate() {
                if (!longpressTime) {
                    *this = RemoteControl();
                }
                if (!autoSleepTime) {
                    autoSleepTime = 2;
                }
            }

            static void defaults();
            static RemoteControl get();
        };

        // --------------------------------------------------------------------
        // IOT Switch

        class IOTSwitch {
        public:
            typedef enum : uint8_t {
                OFF =       0x00,
                ON =        0x01,
                RESTORE =   0x02,
            } StateEnum_t;
            typedef enum : uint8_t {
                NONE =      0x00,
                HIDE =      0x01,
                NEW_ROW =   0x02,
            } WebUIEnum_t;
            typedef struct __attribute__packed__ {
                uint8_t length;
                StateEnum_t state;
                WebUIEnum_t webUI;
            } Switch_t;

            static const uint8_t *getConfig();
            static void setConfig(const uint8_t *buf, size_t size);

            // T = std::array<String, N>, R = std::array<Switch_t, N>
            template <class T, class R>
            static void getConfig(T &names, R &configs) {
                names = {};
                configs = {};
                uint16_t length = 0;
                auto ptr = getConfig();
                if (ptr) {
                    size_t i = 0;
                    auto endPtr = ptr + length;
                    while(ptr + sizeof(Switch_t) <= endPtr && i < names.size()) {
                        configs[i] = *reinterpret_cast<Switch_t *>(const_cast<uint8_t *>(ptr));
                        ptr += sizeof(Switch_t);
                        if (ptr + configs[i].length <= endPtr) {
                            names[i] = PrintString(ptr, configs[i].length);
                        }
                        ptr += configs[i++].length;
                    }
                }
            }

            template <class T, class R>
            static void setConfig(const T &names, R &configs) {
                Buffer buffer;
                for(size_t i = 0; i < names.size(); i++) {
                    configs[i].length = names[i].length();
                    buffer.writeObject(configs[i]);
                    buffer.write(names[i]);
                }
                setConfig(buffer.begin(), buffer.length());
            }
        };

        // --------------------------------------------------------------------
        // Weather Station

        class WeatherStation
        {
        public:
            class WeatherStationConfig {
            public:
                WeatherStationConfig();
                void validate();
                uint32_t getPollIntervalMillis();
                struct __attribute__packed__ {
                    uint16_t weather_poll_interval;
                    uint16_t api_timeout;
                    uint8_t backlight_level;
                    uint8_t touch_threshold;
                    uint8_t released_threshold;
                    uint8_t is_metric: 1;
                    uint8_t time_format_24h: 1;
                    uint8_t show_webui: 1;
                    float temp_offset;
                    float humidity_offset;
                    float pressure_offset;
                    uint8_t screenTimer[8];
                };
            };

            WeatherStation();

            static void defaults();
            static const char *getApiKey();
            static const char *getQueryString();
            static WeatherStationConfig &getWriteableConfig();
            static WeatherStationConfig getConfig();
            void setConfig(const WeatherStationConfig &params);

            char openweather_api_key[65];
            char openweather_api_query[65];
            WeatherStationConfig config;
        };


        // --------------------------------------------------------------------
        // Alarm

        #ifndef IOT_ALARM_PLUGIN_MAX_ALERTS
        #define IOT_ALARM_PLUGIN_MAX_ALERTS                         10
        #endif
        #ifndef IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION
        #define IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION               300
        #endif

        class AlarmConfig
        {
        public:
            static constexpr uint8_t MAX_ALARMS = IOT_ALARM_PLUGIN_MAX_ALERTS;
            static constexpr uint16_t DEFAULT_MAX_DURATION = IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION;
            static constexpr uint16_t STOP_ALARM = std::numeric_limits<uint16_t>::max() - 1;

            using TimeType = uint32_t;

            enum class AlarmModeType : uint8_t {
                BOTH,       // can be used if silent or buzzer is not available
                SILENT,
                BUZZER
            };

            enum class WeekDaysType : uint8_t {
                NONE,
                SUNDAY = _BV(0),
                MONDAY = _BV(1),
                TUESDAY = _BV(2),
                WEDNESDAY = _BV(3),
                THURSDAY = _BV(4),
                FRIDAY = _BV(5),
                SATURDAY = _BV(6),
                WEEK_DAYS = _BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5),
                WEEK_END = _BV(0)|_BV(6),
            };

            typedef union __attribute__packed__ {
                WeekDaysType week_days_enum;
                uint8_t week_days;
                struct {
                    uint8_t sunday: 1;              // 0
                    uint8_t monday: 1;              // 1
                    uint8_t tuesday: 1;             // 2
                    uint8_t wednesday: 1;           // 3
                    uint8_t thursday: 1;            // 4
                    uint8_t friday: 1;              // 5
                    uint8_t saturday: 1;            // 6
                };
            } WeekDay_t;

            typedef struct __attribute__packed__ {
                TimeType timestamp;
                struct __attribute__packed__ {
                    uint8_t hour;
                    uint8_t minute;
                };
                WeekDay_t week_day;
            } AlarmTime_t;

            typedef struct __attribute__packed__ {
                AlarmTime_t time;
                union {
                    AlarmModeType mode_type;
                    uint8_t mode;
                };
                uint16_t max_duration: 15;       // limit in seconds, 0 = unlimited
                uint16_t is_enabled: 1;
            } SingleAlarm_t;

            typedef struct __attribute__packed__ {
                SingleAlarm_t alarms[MAX_ALARMS];
            } Alarm_t;
        };

        class Alarm : public AlarmConfig, public ConfigGetterSetter<AlarmConfig::Alarm_t, _H(MainConfig().plugins.alarm.cfg) CIF_DEBUG(, &handleNameAlarm_t)> {
        public:

            Alarm();

            // update timestamp for a single alarm
            // set now to the current time plus a safety margin to let the system install the alarm
            // auto now = time(nullptr) + 30;
            // auto tm = localtime(&now);
            // - if the alarm is disabled, timestamp is set to 0
            // - if any weekday is selected, the timestamp is set to 0
            // - if none of the weekdays are selected, the timestamp is set to unixtime at hour:minute of today. if hour:minute has
            // passed already, the alarm is set for tomorrow (+1 day) at hour:minute
            static void updateTimestamp(const struct tm *tm, SingleAlarm_t &alarm);

            // calls updateTimestamp() for each entry
            static void updateTimestamps(const struct tm *tm, Alarm_t &cfg);

            // return time of the next alarm
            static TimeType getTime(const struct tm *tm, SingleAlarm_t &alarm);

            static void getWeekDays(Print &output, uint8_t weekdays, char none = 'x');
            static String getWeekDaysString(uint8_t weekdays, char none = 'x');

            static void defaults();
            static void dump(Print &output, Alarm_t &cfg);

            Alarm_t cfg;
        };

        // --------------------------------------------------------------------
        // Serial2TCP

        class Serial2TCPConfig {
        public:
            enum class ModeType : uint8_t {
                NONE,
                SERVER,
                CLIENT
            };
            enum class SerialPortType : uint8_t {
                SERIAL0,
                SERIAL1,
                SOFTWARE
            };

            typedef struct __attribute__packed__ Serial2Tcp_t {
                uint16_t port;
                uint32_t baudrate;
                union __attribute__packed__ {
                    ModeType mode;
                    uint8_t mode_byte;
                };
                union __attribute__packed__ {
                    SerialPortType serial_port;
                    uint8_t serial_port_byte;
                };
                uint8_t rx_pin;
                uint8_t tx_pin;
                bool authentication;
                bool auto_connect;
                uint8_t keep_alive;
                uint8_t auto_reconnect;
                uint16_t idle_timeout;

                Serial2Tcp_t() : port(2323), baudrate(KFC_SERIAL_RATE), mode(ModeType::SERVER), serial_port(SerialPortType::SERIAL0), rx_pin(0), tx_pin(0), authentication(false), auto_connect(false), keep_alive(30), auto_reconnect(5), idle_timeout(300) {}

            } Serial2Tcp_t;
        };

        class Serial2TCP : public Serial2TCPConfig, ConfigGetterSetter<Serial2TCPConfig::Serial2Tcp_t, _H(MainConfig().plugins.serial2tcp.cfg) CIF_DEBUG(, &handleNameSerial2TCPConfig_t)> {
        public:

            static void defaults();
            static bool isEnabled();

            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.serial2tcp, Hostname, 64);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.serial2tcp, Username, 32);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.serial2tcp, Password, 32);
        };

        // --------------------------------------------------------------------
        // MQTTClient

        class MQTTClientConfig {
        public:
            enum class ModeType : uint8_t {
                MIN = 0,
                DISABLED = MIN,
                UNSECURE,
                SECURE,
                MAX
            };

            enum class QosType : uint8_t {
                MIN = 0,
                AT_MODE_ONCE = 0,
                AT_LEAST_ONCE = 1,
                EXACTLY_ONCE = 2,
                MAX,
                DEFAULT = 0xff,
            };

            typedef struct __attribute__packed__ MqttConfig_t {
                uint16_t port;
                uint8_t keepalive;
                union __attribute__packed__ {
                    ModeType mode_enum;
                    uint8_t mode;
                };
                union __attribute__packed__ {
                    QosType qos_enum;
                    uint8_t qos;
                };
                uint8_t auto_discovery: 1;

                template<class T>
                void dump() {
                    CONFIG_DUMP_STRUCT_INFO(T);
                    CONFIG_DUMP_STRUCT_VAR(port);
                    CONFIG_DUMP_STRUCT_VAR(keepalive);
                    CONFIG_DUMP_STRUCT_VAR(mode);
                    CONFIG_DUMP_STRUCT_VAR(qos);
                    CONFIG_DUMP_STRUCT_VAR(auto_discovery);
                }

                MqttConfig_t() : port(1883), keepalive(15), mode_enum(ModeType::UNSECURE), qos_enum(QosType::EXACTLY_ONCE), auto_discovery(true) {}

            } MqttConfig_t;
        };

        class MQTTClient : public MQTTClientConfig, public ConfigGetterSetter<MQTTClientConfig::MqttConfig_t, _H(MainConfig().plugins.mqtt.cfg) CIF_DEBUG(, &handleNameMqttConfig_t)> {
        public:
            static void defaults();
            static bool isEnabled();

            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.mqtt, Hostname, 128);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.mqtt, Username, 32);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.mqtt, Password, 32);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.mqtt, Topic, 32);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.mqtt, AutoDiscoveryPrefix, 32);

            static const uint8_t *getFingerPrint(uint16_t &size);
            static void setFingerPrint(const uint8_t *fingerprint, uint16_t size);
            static constexpr size_t kFingerprintMaxSize = 20;
        };

        // --------------------------------------------------------------------
        // Syslog
        class SyslogClientConfig {
        public:
            using SyslogProtocolType = ::SyslogProtocolType;
            typedef struct __attribute__packed__ SyslogConfig_t {
                uint16_t port;
                union __attribute__packed__ {
                    SyslogProtocolType protocol_enum;
                    uint8_t protocol;
                };

                template<class T>
                void dump() {
                    CONFIG_DUMP_STRUCT_INFO(T);
                    CONFIG_DUMP_STRUCT_VAR(port);
                    CONFIG_DUMP_STRUCT_VAR(protocol);
                }

                SyslogConfig_t() : port(514), protocol_enum(SyslogProtocolType::TCP) {}

            } SyslogConfig_t;
        };

        class SyslogClient : public SyslogClientConfig, public ConfigGetterSetter<SyslogClientConfig::SyslogConfig_t, _H(MainConfig().plugins.syslog.cfg) CIF_DEBUG(, &handleNameSyslogConfig_t)>
        {
        public:
            SyslogClient() {}

            static void defaults();
            static bool isEnabled();
            static bool isEnabled(SyslogProtocolType protocol);

            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.syslog, Hostname, 128);
        };

        // --------------------------------------------------------------------
        // Plugin Structure

        HomeAssistant homeassistant;
        RemoteControl remotecontrol;
        IOTSwitch iotswitch;
        WeatherStation weatherstation;
        Alarm alarm;
        Serial2TCP serial2tcp;
        MQTTClient mqtt;
        SyslogClient syslog;

    };

    // --------------------------------------------------------------------
    // Main Config Structure

    struct MainConfig {

        System system;
        Network network;
        Plugins plugins;
    };

};

#include <pop_pack.h>

#ifdef _H_DEFINED_KFCCONFIGURATIONCLASSES
#undef CONFIG_GET_HANDLE_STR
#undef _H
#undef _H_DEFINED_KFCCONFIGURATIONCLASSES
#endif
