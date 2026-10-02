/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // Weather Station

        namespace WeatherStationConfigNS {

            enum class ScreenType : uint8_t {
                MAIN = 0,
                INDOOR,
                FORECAST,
                WORLD_CLOCK,
                MOON_PHASE,
                #if HAVE_WEATHER_STATION_ANALOG_CLOCK
                    ANALOG_CLOCK,
                #endif
                #if HAVE_WEATHER_STATION_INFO_SCREEN
                    INFO,
                #endif
                #if HAVE_WEATHER_STATION_CURATED_ART
                    CURATED_ART,
                #endif
                #if DEBUG_IOT_WEATHER_STATION
                    DEBUG_INFO,
                #endif
                NUM_SCREENS,
                TEXT_CLEAR,
                TEXT_UPDATE,
                TEXT,
            };

            struct __attribute__packed__ WorldClockType {
                using Type = WorldClockType;
                CREATE_UINT8_BITFIELD_MIN_MAX(_time_format_24h, 1, false, true, false);
                CREATE_UINT8_BITFIELD_MIN_MAX(_enabled, 1, false, true, false);

                bool isEnabled() const {
                    return _enabled;
                }

                WorldClockType() :
                    _time_format_24h(kDefaultValueFor__time_format_24h),
                    _enabled(kDefaultValueFor__enabled)
                {
                }
            };

            class WeatherStationConfig {
            public:
                struct __attribute__packed__ Config_t {
                    using Type = Config_t;

                    static constexpr auto kNumScreens = static_cast<uint8_t>(ScreenType::NUM_SCREENS);
                    static constexpr uint8_t kSkipScreen = 0xff;
                    static constexpr uint8_t kManualScreen = 0;

                    static constexpr uint8_t kNumClocks = WEATHER_STATION_MAX_CLOCKS;

                    CREATE_FLOAT_FIELD(latitude, -180, 180, 0)
                    CREATE_FLOAT_FIELD(longitude, -180, 180, 0)
                    CREATE_UINT32_BITFIELD_MIN_MAX(weather_poll_interval, 8, 5, 240, 15); // minutes
                    CREATE_UINT32_BITFIELD_MIN_MAX(api_timeout, 9, 10, 300, 30); // seconds
                    CREATE_UINT32_BITFIELD_MIN_MAX(backlight_level, 7, 0, 100, 100); // level in %
                    CREATE_UINT32_BITFIELD_MIN_MAX(touch_threshold, 6, 0, 63, 5);
                    CREATE_UINT32_BITFIELD_MIN_MAX(released_threshold, 6, 0, 63, 8);
                    CREATE_UINT32_BITFIELD_MIN_MAX(is_metric, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(time_format_24h, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(show_regular_clock_on_world_clocks, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(gallery_update_rate, 16, 5, 43200, 120, 30); // seconds
                    uint8_t screenTimer[kNumScreens]; // seconds
                    WorldClockType additionalClocks[kNumClocks];

                    uint32_t getPollIntervalMillis() const {
                        return weather_poll_interval * 60000UL;
                    }

                    Config_t() :
                        latitude(kDefaultValueFor_latitude),
                        longitude(kDefaultValueFor_longitude),
                        weather_poll_interval(kDefaultValueFor_weather_poll_interval),
                        api_timeout(kDefaultValueFor_api_timeout),
                        backlight_level(kDefaultValueFor_backlight_level),
                        touch_threshold(kDefaultValueFor_touch_threshold),
                        released_threshold(kDefaultValueFor_released_threshold),
                        is_metric(kDefaultValueFor_is_metric),
                        time_format_24h(kDefaultValueFor_time_format_24h),
                        show_regular_clock_on_world_clocks(kDefaultValueFor_show_regular_clock_on_world_clocks),
                        gallery_update_rate(kDefaultValueFor_gallery_update_rate),
                        screenTimer{
                            10 /*ScreenType::MAIN*/,
                            10 /*INDOOR*/,
                            kSkipScreen /*FORECAST*/,
                            10 /*WORLD_CLOCK*/,
                            10 /*MOON_PHASE*/
                            #if HAVE_WEATHER_STATION_ANALOG_CLOCK
                                , kManualScreen /*ANALOG_CLOCK*/
                            #endif
                            #if HAVE_WEATHER_STATION_INFO_SCREEN
                                , kManualScreen /*INFO*/
                            #endif
                            #if HAVE_WEATHER_STATION_CURATED_ART
                                , kManualScreen /*CURATED_ART*/
                            #endif
                            #if DEBUG_IOT_WEATHER_STATION
                                , kManualScreen /*DEBUG_INFO*/
                            #endif
                        },
                        additionalClocks{}

                    {
                    }
                };
            };

            class WeatherStation : public WeatherStationConfig, public KFCConfigurationClasses::ConfigGetterSetter<WeatherStationConfig::Config_t, _H(MainConfig().plugins.weatherstation.cfg) CIF_DEBUG(, &handleNameWeatherStationConfig_t)> {
            public:
                static void defaults();

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, ApiKey, 0, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, Location, 0, 64);

                // Source of the indoor metrics of the weather station 2.x plugin
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, TemperatureSource, 0, 128);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, HumiditySource, 0, 128);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, PressureSource, 0, 128);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, Eco2Source, 0, 128);

                // Power monitor channels of the weather station 2.x plugin. One string per channel,
                // the parts are separated by '|' (a name may contain spaces, not '|'):
                //
                //   none
                //   local|<name>|<remote channel id>    the local INA219 sensor of the sensor plugin
                //   remote|<name>|<remote channel id>   one channel of the TCP power monitor server
                //
                // The remote host and port are shared by all remote channels
                // (see getPowerRemoteHost()/getPowerRemotePort() in ws2_data.h)
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, PowerRemoteHost, 0, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, PowerRemotePort, 0, 8);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, PowerChannel0, 0, 128);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, PowerChannel1, 0, 128);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, PowerChannel2, 0, 128);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, PowerChannel3, 0, 128);

                // Window of the power graph of the weather station 2.x plugin in minutes (1..60),
                // one sample per second per channel. It is its own 1 byte binary parameter: adding
                // it as a bitfield to the packed Config_t grows that blob by a byte, and the blob
                // is read back with a size check - the latitude/longitude, the units and the world
                // clocks would be reset once
                static constexpr uint8_t kPowerGraphMinutesMin = 1;
                static constexpr uint8_t kPowerGraphMinutesMax = 60;
                static constexpr uint8_t kPowerGraphMinutesDefault = 5;
                static constexpr ConfigurationHelper::HandleType kPowerGraphMinutesConfigHandle = CONFIG_GET_HANDLE_STR("weatherstation.powerGraphMinutes");

                inline static uint8_t getPowerGraphMinutes() {
                    REGISTER_HANDLE_NAME("weatherstation.powerGraphMinutes", __DBG__TYPE_GET);
                    uint16_t length = sizeof(uint8_t);
                    const auto data = KFCConfigurationClasses::loadBinaryConfig(kPowerGraphMinutesConfigHandle, length);
                    if (!data || length != sizeof(uint8_t)) {
                        return kPowerGraphMinutesDefault;
                    }
                    const auto value = *static_cast<const uint8_t *>(data);
                    if (value < kPowerGraphMinutesMin || value > kPowerGraphMinutesMax) {
                        return kPowerGraphMinutesDefault;
                    }
                    return value;
                }
                inline static void setPowerGraphMinutes(uint8_t value) {
                    REGISTER_HANDLE_NAME("weatherstation.powerGraphMinutes", __DBG__TYPE_SET);
                    if (value < kPowerGraphMinutesMin) {
                        value = kPowerGraphMinutesMin;
                    }
                    else if (value > kPowerGraphMinutesMax) {
                        value = kPowerGraphMinutesMax;
                    }
                    KFCConfigurationClasses::storeBinaryConfig(kPowerGraphMinutesConfigHandle, &value, sizeof(value));
                }

                // Orientation of the Home Assistant dashboard screen (the other screens of the
                // weather station 2.x plugin keep the landscape layout). Its own 1 byte binary
                // parameter for the same reason as the window of the power graph above. The values
                // are the Rotation values of the display driver (see wt32_sc01.h)
                enum class HassRotation : uint8_t {
                    LANDSCAPE = 0,          // 480x320
                    PORTRAIT = 1,           // 320x480
                    LANDSCAPE_FLIPPED = 2,  // 480x320, turned 180 degrees
                    PORTRAIT_FLIPPED = 3,   // 320x480, turned 180 degrees
                };
                static constexpr uint8_t kHassRotationMin = static_cast<uint8_t>(HassRotation::LANDSCAPE);
                static constexpr uint8_t kHassRotationMax = static_cast<uint8_t>(HassRotation::PORTRAIT_FLIPPED);
                static constexpr uint8_t kHassRotationDefault = static_cast<uint8_t>(HassRotation::LANDSCAPE);
                static constexpr ConfigurationHelper::HandleType kHassRotationConfigHandle = CONFIG_GET_HANDLE_STR("weatherstation.hassRotation");

                inline static uint8_t getHassRotation() {
                    REGISTER_HANDLE_NAME("weatherstation.hassRotation", __DBG__TYPE_GET);
                    uint16_t length = sizeof(uint8_t);
                    const auto data = KFCConfigurationClasses::loadBinaryConfig(kHassRotationConfigHandle, length);
                    if (!data || length != sizeof(uint8_t)) {
                        return kHassRotationDefault;
                    }
                    const auto value = *static_cast<const uint8_t *>(data);
                    if (value < kHassRotationMin || value > kHassRotationMax) {
                        return kHassRotationDefault;
                    }
                    return value;
                }
                inline static void setHassRotation(uint8_t value) {
                    REGISTER_HANDLE_NAME("weatherstation.hassRotation", __DBG__TYPE_SET);
                    if (value < kHassRotationMin) {
                        value = kHassRotationMin;
                    }
                    else if (value > kHassRotationMax) {
                        value = kHassRotationMax;
                    }
                    KFCConfigurationClasses::storeBinaryConfig(kHassRotationConfigHandle, &value, sizeof(value));
                }

                // Locks the automatic rotation of the Home Assistant dashboard: while it is set the
                // motion sensor (the MPU-6050) does not rotate the display, the quick settings of
                // the dashboard rotate it manually in any case. Its own 1 byte parameter for the
                // same reason as the rotation above
                static constexpr ConfigurationHelper::HandleType kHassRotationLockConfigHandle = CONFIG_GET_HANDLE_STR("weatherstation.hassRotationLock");

                inline static bool getHassRotationLock() {
                    REGISTER_HANDLE_NAME("weatherstation.hassRotationLock", __DBG__TYPE_GET);
                    uint16_t length = sizeof(uint8_t);
                    const auto data = KFCConfigurationClasses::loadBinaryConfig(kHassRotationLockConfigHandle, length);
                    if (!data || length != sizeof(uint8_t)) {
                        return false;
                    }
                    return *static_cast<const uint8_t *>(data) != 0;
                }
                inline static void setHassRotationLock(bool locked) {
                    REGISTER_HANDLE_NAME("weatherstation.hassRotationLock", __DBG__TYPE_SET);
                    const uint8_t value = locked ? 1 : 0;
                    KFCConfigurationClasses::storeBinaryConfig(kHassRotationLockConfigHandle, &value, sizeof(value));
                }

                #if WEATHER_STATION_MAX_CLOCKS
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, TZ0, 0, 64);
                #endif
                #if WEATHER_STATION_MAX_CLOCKS > 1
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, TZ1, 0, 64);
                #endif
                #if WEATHER_STATION_MAX_CLOCKS > 2
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, TZ2, 0, 64);
                #endif
                #if WEATHER_STATION_MAX_CLOCKS > 3
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, TZ3, 0, 64);
                #endif

                enum class TZPartType {
                    NAME,
                    TZ,
                    TZ_NAME,
                };

                #if WEATHER_STATION_MAX_CLOCKS
                    template<TZPartType _Part>
                    static String _explode(const char *strList) {
                        StringVector list;
                        explode(strList, '\xff', list, 3);
                        list.resize(3);
                        return list.at(static_cast<uint8_t>(_Part));
                    }

                    template<TZPartType _Part>
                    static String _implode(const char *strList, const char *str) {
                        StringVector list;
                        explode(strList, '\xff', list, 3);
                        list.resize(3);
                        list.at(static_cast<uint8_t>(_Part)) = str;
                        return implode('\xff', list, 3);
                    }

                    template<TZPartType _Part>
                    static String _getTZ(uint8_t num) {
                        switch(num) {
                            case 0:
                                return _explode<_Part>(WeatherStation::getTZ0());
                            case 1:
                                return _explode<_Part>(WeatherStation::getTZ1());
                            case 2:
                                return _explode<_Part>(WeatherStation::getTZ2());
                            case 3:
                                return _explode<_Part>(WeatherStation::getTZ3());
                            default:
                                break;
                        }
                        __LDBG_printf("invalid num=%u", num);
                        return String();
                    }

                    template<TZPartType _Part>
                    static void _setTZ(uint8_t num, const char *str) {
                        switch(num) {
                            case 0:
                                WeatherStation::setTZ0(_implode<_Part>(WeatherStation::getTZ0(), str));
                                break;
                            case 1:
                                WeatherStation::setTZ1(_implode<_Part>(WeatherStation::getTZ1(), str));
                                break;
                            case 2:
                                WeatherStation::setTZ2(_implode<_Part>(WeatherStation::getTZ2(), str));
                                break;
                            case 3:
                                WeatherStation::setTZ3(_implode<_Part>(WeatherStation::getTZ3(), str));
                                break;
                            default:
                                break;
                        }
                        __LDBG_printf("invalid num=%u", num);
                    }

                    static String getName(uint8_t num) {
                        return _getTZ<TZPartType::NAME>(num);
                    }

                    static void setName(uint8_t num, const char *str) {
                        _setTZ<TZPartType::NAME>(num, str);
                    }

                    static String getTZ(uint8_t num) {
                        return _getTZ<TZPartType::TZ>(num);
                    }

                    static void setTZ(uint8_t num, const char *str) {
                        _setTZ<TZPartType::TZ>(num, str);
                    }

                    static String getTZName(uint8_t num) {
                        return _getTZ<TZPartType::TZ_NAME>(num);
                    }

                    static void setTZName(uint8_t num, const char *str) {
                        _setTZ<TZPartType::TZ_NAME>(num, str);
                    }

                #endif

            };

        }
    }
}
