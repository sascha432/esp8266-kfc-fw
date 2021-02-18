/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

#ifndef IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_CALIBRATION
#define IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_CALIBRATION          1.0
#endif

namespace KFCConfigurationClasses {

#if IOT_SENSOR_HAVE_BATTERY
    Plugins::Sensor::BatteryConfig_t::BatteryConfig_t() :
        calibration(IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_CALIBRATION),
        offset(0),
        precision(kDefaultValueFor_precision)
    {
    }
#endif

#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
#if defined(IOT_SENSOR_HLW8012_U)
    Plugins::Sensor::SensorConfig_t::HLW80xxConfig_t::HLW80xxConfig_t() : calibrationU(IOT_SENSOR_HLW8012_U), calibrationI(IOT_SENSOR_HLW8012_I), calibrationP(IOT_SENSOR_HLW8012_P), energyCounter(0), extraDigits(kDefaultValueFor_extraDigits)
#else
    Plugins::Sensor::SensorConfig_t::HLW80xxConfig_t::HLW80xxConfig_t() : calibrationU(1), calibrationI(1), calibrationP(1), energyCounter(0), extraDigits(kDefaultValueFor_extraDigits)
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
