/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

        // --------------------------------------------------------------------
        // Sensor

// send recorded data over UDP
#ifndef IOT_SENSOR_HAVE_BATTERY_RECORDER
#define IOT_SENSOR_HAVE_BATTERY_RECORDER                        0
#endif
        class SensorConfig {
        public:

#if IOT_SENSOR_HAVE_BATTERY
            enum class SensorRecordType : uint8_t {
                MIN = 0,
                NONE = MIN,
                ADC = 0x01,
                SENSOR = 0x02,
                BOTH = 0x03,
                MAX
            };

            typedef struct __attribute__packed__ BatteryConfig_t {
                using Type = BatteryConfig_t;

                CREATE_FLOAT_FIELD(calibration, -100, 100, IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_CALIBRATION);
                CREATE_FLOAT_FIELD(offset, -100, 100, 0);
                CREATE_UINT8_BITFIELD_MIN_MAX(precision, 4, 0, 7, 2, 1);

#if IOT_SENSOR_HAVE_BATTERY_RECORDER
                CREATE_ENUM_BITFIELD(record, SensorRecordType);
                CREATE_UINT16_BITFIELD_MIN_MAX(port, 16, 0, 0xffff, 2, 1);
                CREATE_IPV4_ADDRESS(address);
#endif

                BatteryConfig_t();

            } BatteryConfig_t;
#endif

#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
            typedef struct __attribute__packed__ HLW80xxConfig_t {

                using Type = HLW80xxConfig_t;
                float calibrationU;
                float calibrationI;
                float calibrationP;
                uint64_t energyCounter;
                CREATE_UINT8_BITFIELD_MIN_MAX(extraDigits, 4, 0, 7, 0, 1);
                HLW80xxConfig_t();

            } HLW80xxConfig_t;
#endif

#if IOT_SENSOR_HAVE_INA219
            enum class INA219CurrentDisplayType : uint8_t {
                MILLIAMPERE,
                AMPERE,
                MAX
            };

            enum class INA219PowerDisplayType : uint8_t {
                MILLIWATT,
                WATT,
                MAX
            };

            typedef struct __attribute__packed__ INA219Config_t {
                using Type = INA219Config_t;

                CREATE_ENUM_BITFIELD(display_current, INA219CurrentDisplayType);
                CREATE_ENUM_BITFIELD(display_power, INA219PowerDisplayType);
                CREATE_BOOL_BITFIELD_MIN_MAX(webui_current, true);
                CREATE_BOOL_BITFIELD_MIN_MAX(webui_average, true);
                CREATE_BOOL_BITFIELD_MIN_MAX(webui_peak, true);
                CREATE_UINT8_BITFIELD_MIN_MAX(webui_voltage_precision, 3, 0, 6, 2, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(webui_current_precision, 3, 0, 6, 0, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(webui_power_precision, 3, 0, 6, 0, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(webui_update_rate, 4, 1, 15, 2, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(averaging_period, 10, 5, 900, 30, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(hold_peak_time, 10, 5, 900, 60, 1);

                INA219CurrentDisplayType getDisplayCurrent() const {
                    return static_cast<INA219CurrentDisplayType>(display_current);
                }

                INA219PowerDisplayType getDisplayPower() const {
                    return static_cast<INA219PowerDisplayType>(display_power);
                }

                uint32_t getHoldPeakTimeMillis() const {
                    return hold_peak_time * 1000;
                }

                INA219Config_t();

            } INA219Config_t;
#endif

            typedef struct __attribute__packed__ SensorConfig_t {

#if IOT_SENSOR_HAVE_BATTERY
                BatteryConfig_t battery;
#endif
#if IOT_SENSOR_HAVE_INA219
                INA219Config_t ina219;
#endif
#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
                HLW80xxConfig_t hlw80xx;
#endif

            } SensorConfig_t;

        };

        class Sensor : public SensorConfig, public KFCConfigurationClasses::ConfigGetterSetter<SensorConfig::SensorConfig_t, _H(MainConfig().plugins.sensor.cfg) CIF_DEBUG(, &handleNameSensorConfig_t)> {
        public:
            static void defaults();

#if IOT_SENSOR_HAVE_BATTERY_RECORDER
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.sensor, RecordSensorData, 0, 128);

            static void setRecordHostAndPort(String host, uint16_t port) {
                if (port) {
                    host += ':';
                    host + String(port);
                }
                setRecordSensorData(host);
            }
            static String getRecordHost() {
                String str = getRecordSensorData();
                auto pos = str.lastIndexOf(':');
                if (pos != -1) {
                    str.remove(pos);
                }
                return str;
            }
            static uint16_t getRecordPort() {
                auto str = getRecordSensorData();
                auto pos = strrchr(str, ':');
                if (!pos) {
                    return 0;
                }
                return atoi(pos + 1);
            }
#endif
        };
