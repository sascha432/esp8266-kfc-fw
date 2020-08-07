/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

#if IOT_SENSOR_HAVE_BATTERY
    Plugins::Sensor::SensorConfig_t::BatteryConfig_t::BatteryConfig_t() : calibration(1), precision(1), offset(0)
    {
    }
#endif

#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
#if defined(IOT_SENSOR_HLW8012_U)
    Plugins::Sensor::SensorConfig_t::HLW80xxConfig_t::HLW80xxConfig_t() : calibrationU(IOT_SENSOR_HLW8012_U), calibrationI(IOT_SENSOR_HLW8012_I), calibrationP(IOT_SENSOR_HLW8012_P), energyCounter(0), extraDigits(0)
#else
    Plugins::Sensor::SensorConfig_t::HLW80xxConfig_t::HLW80xxConfig_t() : calibrationU(1), calibrationI(1), calibrationP(1), energyCounter(0), extraDigits(0)
#endif
    {
    }
#endif

    void Plugins::Sensor::defaults()
    {
        SensorConfig_t tmp = {};
        setConfig(tmp);
    }

}
