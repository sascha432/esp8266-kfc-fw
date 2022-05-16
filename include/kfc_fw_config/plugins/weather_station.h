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
                    CREATE_UINT32_BITFIELD_MIN_MAX(released_threshold, 6, 0, 63, 8);
                    CREATE_UINT32_BITFIELD_MIN_MAX(is_metric, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(time_format_24h, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(show_webui, 1, false, true, false);
                    CREATE_UINT32_BITFIELD_MIN_MAX(show_regular_clock_on_world_clocks, 1, false, true, true);
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
                        screenTimer{
                            10 /*ScreenType::MAIN*/,
                            10 /*INDOOR*/,
                            kSkipScreen /*FORECAST*/,
                            10 /*WORLD_CLOCK*/,
                            kManualScreen /*INFO*/
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
                };

                #if WEATHER_STATION_MAX_CLOCKS
                    template<TZPartType Part>
                    static String _split(const char *str) {
                        auto sep = strchr(str, 0xff);
                        if (!sep) {
                            return String();
                        }
                        if __CONSTEXPR17 (Part == TZPartType::NAME) {
                            String res;
                            res.concat(str, (sep - str));
                            return res;
                        }
                        else {
                            return String(sep + 1);
                        }
                    }

                    template<TZPartType Part>
                    static String _set(const char *str) {
                        if __CONSTEXPR17 (Part == TZPartType::NAME) {
                            auto name = _split<TZPartType::TZ>(str);
                            return PrintString(F("%s\xff%s"), name.c_str(), str);
                        }
                        else {
                            auto tz = _split<TZPartType::TZ>(str);
                            return PrintString(F("%s\xff%s"), str, tz.c_str());
                        }
                    }

                    template<TZPartType Part>
                    static String _getTZ(uint8_t num) {
                        switch(num) {
                            case 0:
                                return _split<Part>(getTZ0());
                            case 1:
                                return _split<Part>(getTZ1());
                            case 2:
                                return _split<Part>(getTZ2());
                            case 3:
                                return _split<Part>(getTZ3());
                            default:
                                break;
                        }
                        __DBG_printf("invalid num=%u", num);
                        return String();
                    }

                    static String getTZ(uint8_t num) {
                        return _getTZ<TZPartType::TZ>(num);
                    }

                    static String getName(uint8_t num) {
                        return _getTZ<TZPartType::NAME>(num);
                    }

                #endif

            };

        }
    }
}
