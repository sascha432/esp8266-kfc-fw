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

#define CREATE_ZERO_CONF_DEFAULT(service, proto, variable, default_value)   "${zeroconf:_" service "._" proto "," variable "|" default_value "}"
#define CREATE_ZERO_CONF_NO_DEFAULT(service, proto, variable)               "${zeroconf:_" service "._" proto "," variable "}"


#ifndef _H
#define _H_DEFINED_KFCCONFIGURATIONCLASSES                                  1
#define _H(name)                                                            constexpr_crc16_update(_STRINGIFY(name), constexpr_strlen(_STRINGIFY(name)))
#define CONFIG_GET_HANDLE_STR(name)                                         constexpr_crc16_update(name, constexpr_strlen(name))
#endif

#define CREATE_STRING_GETTER_SETTER(class_name, name, len) \
    static constexpr size_t k##name##MaxSize = len; \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static const char *get##name() { return loadStringConfig(k##name##ConfigHandle); } \
    static void set##name(const char *str) { storeStringConfig(k##name##ConfigHandle, str); }


namespace KFCConfigurationClasses {

    using HandleType = uint16_t;

    const void *loadBinaryConfig(HandleType handle, uint16_t length);
    void *loadWriteableBinaryConfig(HandleType handle, uint16_t length);
    void storeBinaryConfig(HandleType handle, const void *data, uint16_t length);
    const char *loadStringConfig(HandleType handle);
    void storeStringConfig(HandleType handle, const char *);

    template<typename ConfigType, HandleType handleArg>
    class ConfigGetterSetter {
    public:
        static constexpr uint16_t kConfigStructHandle = handleArg;
        using ConfigStructType = ConfigType;

        static ConfigType getConfig()
        {
            __CDBG_printf("getConfig=%04x size=%u", kConfigStructHandle, sizeof(ConfigType));
            return *reinterpret_cast<const ConfigType *>(loadBinaryConfig(kConfigStructHandle, sizeof(ConfigType)));
        }

        static void setConfig(const ConfigType &params)
        {
            __CDBG_printf("setConfig=%04x size=%u", kConfigStructHandle, sizeof(ConfigType));
            storeBinaryConfig(kConfigStructHandle, &params, sizeof(params));
        }

        static ConfigType &getWriteableConfig()
        {
            __CDBG_printf("getWriteableConfig=%04x size=%u", kConfigStructHandle, sizeof(ConfigType));
            return *reinterpret_cast<ConfigType *>(loadWriteableBinaryConfig(kConfigStructHandle, sizeof(ConfigType)));
        }
    };

    struct System {

        // --------------------------------------------------------------------
        // Flags

        class Flags {
        public:
            Flags();
            Flags(ConfigFlags flags) : _flags(flags) {
            }
            ConfigFlags *operator->() {
                return &_flags;
            }
            static Flags read();
            static ConfigFlags get();
            static ConfigFlags &getWriteable();
            static void set(ConfigFlags flags);
            void write();

            bool isWiFiEnabled() const {
                return _flags.wifiMode & WIFI_AP_STA;
            }
            bool isSoftAPEnabled() const {
                return _flags.wifiMode & WIFI_AP;
            }
            bool isStationEnabled() const {
                return _flags.wifiMode & WIFI_STA;
            }
            bool isSoftApStandByModeEnabled() const {
                return _flags.apStandByMode;
            }
            bool isMDNSEnabled() const {
                return _flags.enableMDNS;
            }
            void setMDNSEnabled(bool state) {
                _flags.enableMDNS = state;
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

        class Device {
        public:
            enum class StatusLEDModeEnum : uint8_t {
                OFF_WHEN_CONNECTED = 0,
                SOLID_WHEN_CONNECTED = 1,
            };

        public:
            Device() {
            }
            static void defaults();

            static const char *getName();
            static const char *getTitle();
            static const char *getPassword();
            static const char *getToken();
            static constexpr size_t kTokenMinLength = SESSION_DEVICE_TOKEN_MIN_LENGTH;
            static void setName(const String &name);
            static void setTitle(const String &title);
            static void setPassword(const String &password);
            static void setToken(const String &token);

            static void setSafeModeRebootTime(uint16_t minutes);
            static uint16_t getSafeModeRebootTime();
            static void setWebUIKeepLoggedInDays(uint16_t days);
            static uint16_t getWebUIKeepLoggedInDays();
            static uint32_t getWebUIKeepLoggedInSeconds();
            static void setStatusLedMode(StatusLEDModeEnum mode);
            static StatusLEDModeEnum getStatusLedMode();

        public:
            typedef struct __attribute__packed__ {
                uint16_t _safeModeRebootTime;
                uint16_t _webUIKeepLoggedInDays: 15;
                uint16_t _statusLedMode: 1;
            } DeviceSettings_t;

            DeviceSettings_t settings;
        };

        // --------------------------------------------------------------------
        // Firmware

        class Firmware {
        public:
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

            uint8_t elf_sha1[20];
        };

        Flags flags;
        Device device;
        Firmware firmware;

    };

    // --------------------------------------------------------------------
    // Network

    struct Network {

        // --------------------------------------------------------------------
        // Settings

        class Settings {
        public:
            Settings();
            static Settings read();
            static void defaults();
            void write();

            IPAddress localIp() const {
                return _localIp;
            }
            IPAddress subnet() const {
                return _subnet;
            }
            IPAddress gateway() const {
                return _gateway;
            }
            IPAddress dns1() const {
                return _dns1;
            }
            IPAddress dns2() const {
                return _dns2;
            }

            struct __attribute__packed__ {
                uint32_t _localIp;
                uint32_t _subnet;
                uint32_t _gateway;
                uint32_t _dns1;
                uint32_t _dns2;
            };
        };

        // --------------------------------------------------------------------
        // SoftAP

        class SoftAP {
        public:
            using EncryptionType = WiFiEncryptionType;

            SoftAP();
            static SoftAP read();
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
            static const char *getSSID();
            static const char *getPassword();
            static void setSSID(const String &ssid);
            static void setPassword(const String &password);

            static const char *getSoftApSSID();
            static const char *getSoftApPassword();
            static void setSoftApSSID(const String &ssid);
            static void setSoftApPassword(const String &password);

            char _ssid[33];
            char _password[33];
            char _softApSsid[33];
            char _softApPassword[33];
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

        class Alarm : public AlarmConfig, public ConfigGetterSetter<AlarmConfig::Alarm_t, _H(MainConfig().plugins.alarm.cfg)> {
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

        class Serial2TCP : public Serial2TCPConfig, ConfigGetterSetter<Serial2TCPConfig::Serial2Tcp_t, _H(MainConfig().plugins.serial2tcp.cfg)> {
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

        class MQTTClient : public MQTTClientConfig, public ConfigGetterSetter<MQTTClientConfig::MqttConfig_t, _H(MainConfig().plugins.mqtt.cfg)> {
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
            enum class SyslogProtocolType : uint8_t {
                MIN = 0,
                NONE = MIN,
                UDP,
                TCP,
                TCP_TLS,
                FILE,
                MAX
            };
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

        class SyslogClient : public SyslogClientConfig, public ConfigGetterSetter<SyslogClientConfig::SyslogConfig_t, _H(MainConfig().plugins.syslog)>
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
