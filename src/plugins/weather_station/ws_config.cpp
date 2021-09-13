/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    Plugins::WeatherStationConfigNS::WeatherStationConfig::Config_t::Config_t() :
        weather_poll_interval(15),
        api_timeout(30),
        backlight_level(100),
        touch_threshold(5),
        released_threshold(8),
        is_metric(true),
        time_format_24h(true),
        show_webui(false),
        screenTimer{ 10, 10 }
    {
    }

    uint32_t Plugins::WeatherStationConfigNS::WeatherStationConfig::Config_t::getPollIntervalMillis() const
    {
        return weather_poll_interval * 60000UL;
    }

    void Plugins::WeatherStationConfigNS::WeatherStation::defaults()
    {
        setConfig(Config_t());
        setApiKey(F("GET_YOUR_API_KEY_openweathermap.org"));
        setApiQuery(F("New York,US"));
    }

}
