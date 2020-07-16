/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#include <push_pack.h>

namespace KFCConfigurationClasses {

    struct System {

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

        private:
            ConfigFlags _flags;
        };

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
            static void setName(const String &name);
            static void setTitle(const String &title);
            static void setPassword(const String &password);
            static void setToken(const String &token);

            static void setSafeModeRebootTime(uint16_t minutes);
            static uint16_t getSafeModeRebootTime();
            static void setWebUIKeepLoggedInDays(uint8_t days);
            static uint8_t getWebUIKeepLoggedInDays();
            static uint32_t getWebUIKeepLoggedInSeconds();
            static void setStatusLedMode(StatusLEDModeEnum mode);
            static StatusLEDModeEnum getStatusLedMode();

        public:
            typedef struct __attribute__packed__ {
                uint16_t _safeModeRebootTime;
                uint8_t _webUIKeepLoggedInDays;
                uint8_t _statusLedMode: 1;
            } DeviceSettings_t;

            DeviceSettings_t settings;
        };

        Flags flags;
        Device device;

    };

    struct Network {

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

    struct Plugins {

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

            // T = std::array<String, N>, R = std::array<Switch_t, N>
            template <class T, class R>
            static void getConfig(T &names, R &configs) {
                names = {};
                configs = {};
                uint16_t length = 0;
                auto ptr = config.getBinary(_H(MainConfig().plugins.iotswitch), length);
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
                config.setBinary(_H(MainConfig().plugins.iotswitch), buffer.begin(), buffer.length());
            }
        };


        // implementation in src\plugins\weather_station\ws_config.cpp

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

            char openweather_api_key[65];
            char openweather_api_query[65];
            WeatherStationConfig config;
        };


        // implementation in src\plugins\alarm\alarm_config.cpp

        #ifndef IOT_ALARM_PLUGIN_MAX_ALERTS
        #define IOT_ALARM_PLUGIN_MAX_ALERTS                         10
        #endif
        #ifndef IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION
        #define IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION               300
        #endif

        class Alarm
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
            static Alarm_t &getWriteableConfig();
            static Alarm_t getConfig();
            static void setConfig(Alarm_t &alarm);

            Alarm_t cfg;
        };


        HomeAssistant homeassistant;
        RemoteControl remotecontrol;
        IOTSwitch iotswitch;
        WeatherStation weatherstation;
        Alarm alarm;

    };

    struct MainConfig {

        System system;
        Network network;
        Plugins plugins;

    };

};

#include <pop_pack.h>
