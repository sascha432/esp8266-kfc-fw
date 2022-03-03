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
                INFO,
                #if DEBUG
                    DEBUG_INFO,
                #endif
                NUM_SCREENS,
                TEXT_CLEAR,
                TEXT_UPDATE,
                TEXT,
            };

            class WeatherStationConfig {
            public:
                struct __attribute__packed__ Config_t {
                    using Type = Config_t;

                    static constexpr auto kNumScreens = static_cast<uint8_t>(ScreenType::NUM_SCREENS);
                    static constexpr uint8_t kSkipScreen = 0xff;

                    CREATE_UINT32_BITFIELD_MIN_MAX(weather_poll_interval, 8, 5, 240, 15); // minutes
                    CREATE_UINT32_BITFIELD_MIN_MAX(api_timeout, 9, 10, 300, 30); // seconds
                    CREATE_UINT32_BITFIELD_MIN_MAX(backlight_level, 7, 0, 100, 100); // level in %
                    CREATE_UINT32_BITFIELD_MIN_MAX(touch_threshold, 6, 0, 63, 5);
                    CREATE_UINT32_BITFIELD_MIN_MAX(released_threshold, 6, 0, 63, 8);
                    CREATE_UINT32_BITFIELD_MIN_MAX(is_metric, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(time_format_24h, 1, false, true, true);
                    CREATE_UINT32_BITFIELD_MIN_MAX(show_webui, 1, false, true, false);
                    uint8_t screenTimer[kNumScreens]; // seconds

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
                        screenTimer{ 10, 10, kSkipScreen, 0 }
                    {}
                };
            };

            class WeatherStation : public WeatherStationConfig, public KFCConfigurationClasses::ConfigGetterSetter<WeatherStationConfig::Config_t, _H(MainConfig().plugins.weatherstation.cfg) CIF_DEBUG(, &handleNameWeatherStationConfig_t)> {
            public:
                static void defaults();

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, ApiKey, 0, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, ApiQuery, 0, 64);

            };

        }
    }
}
