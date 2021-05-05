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
#if IOT_SENSOR_HAVE_BATTERY_RECORDER
        ,
        record(static_cast<uint8_t>(SensorRecordType::NONE)),
        port(kDefaultValueFor_port),
        address(IPADDR4_INIT_BYTES(192, 168, 0, 3))
#endif
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

#if IOT_SENSOR_HAVE_INA219
    Plugins::Sensor::INA219Config_t::INA219Config_t() :
        display_current(0),
        display_power(0),
        webui_current(kDefaultValueFor_webui_current),
        webui_average(kDefaultValueFor_webui_average),
        webui_peak(kDefaultValueFor_webui_peak),
        webui_voltage_precision(kDefaultValueFor_webui_voltage_precision),
        webui_current_precision(kDefaultValueFor_webui_current_precision),
        webui_power_precision(kDefaultValueFor_webui_power_precision),
        averaging_period(kDefaultValueFor_averaging_period),
        hold_peak_time(kDefaultValueFor_hold_peak_time)
    {
    }
#endif

    void Plugins::Sensor::defaults()
    {
        auto tmp = SensorConfig_t();
        setConfig(tmp);
    }

}
