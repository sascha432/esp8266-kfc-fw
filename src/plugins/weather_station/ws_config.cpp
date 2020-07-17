/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include <kfc_fw_config_classes.h>

namespace KFCConfigurationClasses {

    Plugins::WeatherStation::WeatherStationConfig::WeatherStationConfig() :
        weather_poll_interval(15),
        api_timeout(30),
        backlight_level(100),
        touch_threshold(5),
        released_threshold(8),
        is_metric(true),
        time_format_24h(true),
        show_webui(false),
        temp_offset(0),
        humidity_offset(0),
        pressure_offset(0),
        screenTimer()
    {
        screenTimer[0] = 10;
        screenTimer[1] = 10;
    }

    void Plugins::WeatherStation::WeatherStationConfig::validate()
    {
        if (weather_poll_interval == 0 || api_timeout == 0 || touch_threshold == 0 || released_threshold == 0) {
            *this = WeatherStationConfig();
        }
        if (backlight_level < 10) {
            backlight_level = 10;
        }
    }

    uint32_t Plugins::WeatherStation::WeatherStationConfig::getPollIntervalMillis()
    {
        return weather_poll_interval * 60000UL;
    }


    Plugins::WeatherStation::WeatherStation()
    {
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

    Plugins::WeatherStation::WeatherStationConfig &Plugins::WeatherStation::getWriteableConfig()
    {
        return ::config._H_W_GET(MainConfig().plugins.weatherstation.config);
    }

    Plugins::WeatherStation::WeatherStationConfig Plugins::WeatherStation::getConfig()
    {
        return ::config._H_GET(MainConfig().plugins.weatherstation.config);
    }

    void Plugins::WeatherStation::setConfig(const Plugins::WeatherStation::WeatherStationConfig &params)
    {
        ::config._H_SET(MainConfig().plugins.weatherstation.config, params);
    }

}
