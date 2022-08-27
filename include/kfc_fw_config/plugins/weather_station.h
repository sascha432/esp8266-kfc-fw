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
                INFO,
                PICTURES,
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

                    CREATE_UINT32_BITFIELD_MIN_MAX(weather_poll_interval, 8, 5, 240, 15); // minutes
                    CREATE_UINT32_BITFIELD_MIN_MAX(api_timeout, 9, 10, 300, 30); // seconds
                    CREATE_UINT32_BITFIELD_MIN_MAX(backlight_level, 7, 0, 100, 100); // level in %
                    CREATE_UINT32_BITFIELD_MIN_MAX(touch_threshold, 6, 0, 63, 5);
                    CREATE_UINT32_BITFIELD_MIN_MAX(released_threshold, 6, 0, 63, 15);
                    CREATE_UINT32_BITFIELD_MIN_MAX(is_metric, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(time_format_24h, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(show_webui, 1, false, true, false);
                    CREATE_UINT32_BITFIELD_MIN_MAX(show_regular_clock_on_world_clocks, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(gallery_update_rate, 16, 5, 43200, 120, 30); // seconds
                    uint8_t screenTimer[kNumScreens]; // seconds
                    WorldClockType additionalClocks[kNumClocks];

                    uint32_t getPollIntervalMillis() const {
                        return weather_poll_interval * 60000U;
                    }

                    Config_t() :
                        weather_poll_interval(kDefaultValueFor_weather_poll_interval),
                        api_timeout(kDefaultValueFor_api_timeout),
                        backlight_level(kDefaultValueFor_backlight_level),
                        touch_threshold(kDefaultValueFor_touch_threshold),
                        released_threshold(kDefaultValueFor_released_threshold),
                        is_metric(kDefaultValueFor_is_metric),
                        time_format_24h(kDefaultValueFor_time_format_24h),
                        show_webui(kDefaultValueFor_show_webui),
                        show_regular_clock_on_world_clocks(kDefaultValueFor_show_regular_clock_on_world_clocks),
                        gallery_update_rate(kDefaultValueFor_gallery_update_rate),
                        screenTimer{
                            10 /*ScreenType::MAIN*/,
                            10 /*INDOOR*/,
                            kSkipScreen /*FORECAST*/,
                            10 /*WORLD_CLOCK*/,
                            10 /*MOON_PHASE*/,
                            kManualScreen, /*INFO*/
                            kManualScreen /*PICTURES*/
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
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, ApiQuery, 0, 64);

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
