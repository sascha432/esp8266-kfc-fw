/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

        // --------------------------------------------------------------------
        // IOT Switch

        class IOTSwitch {
        public:
            enum class StateEnum : uint8_t {
                OFF =       0x00,
                ON =        0x01,
                RESTORE =   0x02,
            };
            enum class WebUIEnum : uint8_t {
                NONE =      0x00,
                HIDE =      0x01,
                NEW_ROW =   0x02,
                TOP =       0x03,
            };
            struct __attribute__packed__ SwitchConfig {
                using Type = SwitchConfig;

                void setLength(size_t length) {
                    _length = length;
                }
                size_t getLength() const {
                    return _length;
                }
                void setState(StateEnum state) {
                    _state = static_cast<uint8_t>(state);
                }
                StateEnum getState() const {
                    return static_cast<StateEnum>(_state);
                }
                void setWebUI(WebUIEnum webUI) {
                    _webUI = static_cast<uint8_t>(webUI);
                }
                WebUIEnum getWebUI() const {
                    return static_cast<WebUIEnum>(_webUI);
                }
                WebUINS::NamePositionType getWebUINamePosition() const {
                    switch(static_cast<WebUIEnum>(_webUI)) {
                        case WebUIEnum::TOP:
                        case WebUIEnum::NEW_ROW:
                            return WebUINS::NamePositionType::TOP;
                        default:
                            break;
                    }
                    return WebUINS::NamePositionType::SHOW;
                }

                SwitchConfig() : _length(0), _state(0), _webUI(0) {}
                SwitchConfig(const String &name, StateEnum state, WebUIEnum webUI) : _length(name.length()), _state(static_cast<uint8_t>(state)), _webUI(static_cast<uint8_t>(webUI)) {}

                CREATE_UINT8_BITFIELD(_length, 8);
                CREATE_UINT8_BITFIELD(_state, 4);
                CREATE_UINT8_BITFIELD(_webUI, 4);
            };

            static const uint8_t *getConfig(uint16_t &length);
            static void setConfig(const uint8_t *buf, size_t size);

            // T = std::array<String, N>, R = std::array<SwitchConfig, N>
            template <class T, class R>
            static void getConfig(T &names, R &configs) {
                names = {};
                configs = {};
                uint16_t length = 0;
                auto ptr = getConfig(length);
#if DEBUG_IOT_SWITCH
                __dump_binary_to(DEBUG_OUTPUT, ptr, length, length, PSTR("getConfig"));
#endif
                if (ptr) {
                    uint8_t i = 0;
                    auto endPtr = ptr + length;
                    while(ptr + sizeof(SwitchConfig) <= endPtr && i < names.size()) {
                        configs[i] = *reinterpret_cast<SwitchConfig *>(const_cast<uint8_t *>(ptr));
                        ptr += sizeof(SwitchConfig);
                        if (ptr + configs[i].getLength() <= endPtr) {
                            names[i] = PrintString(ptr, configs[i].getLength());
                        }
                        ptr += configs[i++].getLength();
                    }
                }
            }

            template <class T, class R>
            static void setConfig(const T &names, R &configs) {
                Buffer buffer;
                for(uint8_t i = 0; i < names.size(); i++) {
                    configs[i].setLength(names[i].length());
                    buffer.push_back(configs[i]);
                    buffer.write(static_cast<const String &>(names[i]));
                }
                setConfig(buffer.begin(), buffer.length());
#if DEBUG_IOT_SWITCH
                __dump_binary_to(DEBUG_OUTPUT, buffer.begin(), buffer.length(), buffer.length(), PSTR("setConfig"));
#endif
            }
        };

        // --------------------------------------------------------------------
        // Weather Station
        class WeatherStationConfig {
        public:
            typedef struct __attribute__packed__ Config_t {
                using Type = Config_t;

                uint8_t weather_poll_interval;                      // minutes
                uint16_t api_timeout;                               // seconds
                uint8_t backlight_level;                            // PWM level 0-1023
                uint8_t touch_threshold;
                uint8_t released_threshold;
                CREATE_UINT8_BITFIELD(is_metric, 1);
                CREATE_UINT8_BITFIELD(time_format_24h, 1);
                CREATE_UINT8_BITFIELD(show_webui, 1);
                float temp_offset;
                float humidity_offset;
                float pressure_offset;
                uint8_t screenTimer[8];                             // seconds

                Config_t();
            } Config_t;
        };

        class WeatherStation : public WeatherStationConfig, public KFCConfigurationClasses::ConfigGetterSetter<WeatherStationConfig::Config_t, _H(MainConfig().plugins.weatherstation.cfg) CIF_DEBUG(, &handleNameWeatherStationConfig_t)> {
        public:
            static void defaults();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, ApiKey, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, ApiQuery, 0, 64);

        };
