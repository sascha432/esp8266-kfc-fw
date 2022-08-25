/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::WeatherStationConfigNS::WeatherStation::defaults()
    {
        auto cfg = Config_t();
        cfg.additionalClocks[0]._time_format_24h = true;
        cfg.additionalClocks[0]._enabled = true;
        cfg.additionalClocks[1]._enabled = true;
        cfg.additionalClocks[2]._enabled = true;
        setConfig(cfg);
        setTZ0(F("Berlin" "\xff" "CET-1CEST,M3.5.0,M10.5.0/3" "\xff" "Europe/Berlin"));
        setTZ1(F("New York" "\xff" "EST5EDT,M3.2.0,M11.1.0" "\xff" "America/New_York"));
        setTZ2(F("Tokyo" "\xff" "JST-9" "\xff" "Asia/Tokyo"));
        setApiKey(F("GET_YOUR_API_KEY_openweathermap.org"));
        setApiQuery(F("New York,US"));
    }
}
