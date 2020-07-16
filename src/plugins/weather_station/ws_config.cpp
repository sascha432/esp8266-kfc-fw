/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include "kfc_fw_config.h"
#include "kfc_fw_config_classes.h"

namespace KFCConfigurationClasses {

    void Plugins::WeatherStation::WeatherStationConfig_t::reset()
    {
        *this = { 15, 30, 100, 5, 8, false, false, false, 0.0, 0.0, 0.0, { 10, 10, 0, 0, 0, 0, 0, 0 } };
    }

    void Plugins::WeatherStation::WeatherStationConfig_t::validate()
    {
        if (weather_poll_interval == 0 || api_timeout == 0 || touch_threshold == 0 || released_threshold == 0) {
            reset();
        }
        if (backlight_level < 10) {
            backlight_level = 10;
        }
    }

    uint32_t Plugins::WeatherStation::WeatherStationConfig_t::getPollIntervalMillis()
    {
        return weather_poll_interval * 60000UL;
    }


    Plugins::WeatherStation::WeatherStation()
    {
        config.reset();
    }

    void Plugins::WeatherStation::defaults()
    {
        Plugins::WeatherStation ws;
        ::config._H_SET_STR(MainConfig().plugins.weatherstation.openweather_api_key, F("GET_YOUR_API_KEY_openweathermap.org"));
        ::config._H_SET_STR(MainConfig().plugins.weatherstation.openweather_api_query, F("New York,US"));
        ::config._H_SET(MainConfig().plugins.weatherstation.config, ws.config);
    }

    const char *Plugins::WeatherStation::getApiKey()
    {
        return ::config._H_STR(MainConfig().plugins.weatherstation.openweather_api_key);
    }

    const char *Plugins::WeatherStation::getQueryString()
    {
        return ::config._H_STR(MainConfig().plugins.weatherstation.openweather_api_query);
    }

    Plugins::WeatherStation::WeatherStationConfig_t &Plugins::WeatherStation::getWriteableConfig()
    {
        return ::config._H_W_GET(MainConfig().plugins.weatherstation.config);
    }

    Plugins::WeatherStation::WeatherStationConfig_t Plugins::WeatherStation::getConfig()
    {
        return ::config._H_GET(MainConfig().plugins.weatherstation.config);
    }

}
