/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    namespace Plugins {

        namespace SensorConfigNS {

            #if IOT_SENSOR_HAVE_BATTERY
                BatteryConfigType::BatteryConfigType() :
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
                HLW80xxConfigType::HLW80xxConfigType() :
                    #if defined(IOT_SENSOR_HLW8012_U)
                        calibrationU(IOT_SENSOR_HLW8012_U),
                        calibrationI(IOT_SENSOR_HLW8012_I),
                        calibrationP(IOT_SENSOR_HLW8012_P),
                    #else
                        calibrationU(kDefaultValueFor_calibrationU),
                        calibrationI(kDefaultValueFor_calibrationI),
                        calibrationP(kDefaultValueFor_calibrationP),
                    #endif
                    energyCounter(0),
                    extraDigits(kDefaultValueFor_extraDigits)
                {
                }
            #endif

            #if IOT_SENSOR_HAVE_INA219
                INA219ConfigType::INA219ConfigType() :
                    display_current(0),
                    display_power(0),
                    webui_current(kDefaultValueFor_webui_current),
                    webui_average(kDefaultValueFor_webui_average),
                    webui_peak(kDefaultValueFor_webui_peak),
                    webui_voltage_precision(kDefaultValueFor_webui_voltage_precision),
                    webui_current_precision(kDefaultValueFor_webui_current_precision),
                    webui_power_precision(kDefaultValueFor_webui_power_precision),
                    webui_update_rate(kDefaultValueFor_webui_update_rate),
                    averaging_period(kDefaultValueFor_averaging_period),
                    hold_peak_time(kDefaultValueFor_hold_peak_time)
                {
                }
            #endif

            void Sensor::defaults()
            {
                setConfig(ConfigStructType());
            }

        }

    }

}
