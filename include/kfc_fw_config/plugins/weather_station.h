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

            class WeatherStationConfig {
            public:
                struct __attribute__packed__ Config_t {
                    using Type = Config_t;

                    uint8_t weather_poll_interval;                      // minutes
                    uint16_t api_timeout;                               // seconds
                    uint8_t backlight_level;                            // PWM level 0-1023
                    uint8_t touch_threshold;
                    uint8_t released_threshold;
                    CREATE_UINT8_BITFIELD(is_metric, 1);
                    CREATE_UINT8_BITFIELD(time_format_24h, 1);
                    CREATE_UINT8_BITFIELD(show_webui, 1);
                    uint8_t screenTimer[8];                             // seconds

                    uint32_t getPollIntervalMillis() const;

                    Config_t();
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
