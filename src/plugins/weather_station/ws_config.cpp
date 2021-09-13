/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::WeatherStationConfigNS::WeatherStation::defaults()
    {
        setConfig(Config_t());
        setApiKey(F("GET_YOUR_API_KEY_openweathermap.org"));
        setApiQuery(F("New York,US"));
    }

}
