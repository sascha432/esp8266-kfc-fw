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

            struct __attribute__packed__ BatteryConfig_t {
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

            };
#endif

#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
            struct __attribute__packed__ HLW80xxConfig_t {

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

            struct __attribute__packed__ INA219Config_t {
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

            };
#endif

#if IOT_SENSOR_HAVE_MOTION_SENSOR
            struct __attribute__packed__ MotionSensorConfig_t {
                using Type = MotionSensorConfig_t;

#if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
                CREATE_UINT32_BITFIELD_MIN_MAX(motion_auto_off, 10, 0, 1000, 0, 1);
#endif
                CREATE_UINT32_BITFIELD_MIN_MAX(motion_trigger_timeout, 8, 1, 240, 15, 1);

                MotionSensorConfig_t() :
#if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
                    motion_auto_off(kDefaultValueFor_motion_auto_off),
#endif
                    motion_trigger_timeout(kDefaultValueFor_motion_trigger_timeout)
                {
                }

            };
#endif

#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR

            struct __attribute__packed__ AmbientLightSensorConfig_t {
                using Type = AmbientLightSensorConfig_t;

                CREATE_INT32_BITFIELD_MIN_MAX(auto_brightness, 17, -1, 0xffff, -1, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(adjustment_speed, 8, 0, 255, 128, 1);

                AmbientLightSensorConfig_t() :
                    auto_brightness(kDefaultValueFor_auto_brightness)
                {
                }
            };

#endif

#if IOT_SENSOR_HAVE_BME280
            struct __attribute__packed__ BME280Sensor_t {
                using Type = BME280Sensor_t;

                CREATE_FLOAT_FIELD(temp_offset, -100, 100, 0);
                CREATE_FLOAT_FIELD(humidity_offset, -100, 100, 0);
                CREATE_FLOAT_FIELD(pressure_offset, -100, 100, 0);

                BME280Sensor_t() :
                    temp_offset(kDefaultValueFor_temp_offset),
                    humidity_offset(kDefaultValueFor_humidity_offset),
                    pressure_offset(kDefaultValueFor_pressure_offset)
                {
                }

            };
#endif
            struct __attribute__packed__ SensorConfig_t {

#if IOT_SENSOR_HAVE_BATTERY
                BatteryConfig_t battery;
#endif
#if IOT_SENSOR_HAVE_INA219
                INA219Config_t ina219;
#endif
#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
                HLW80xxConfig_t hlw80xx;
#endif
#if IOT_SENSOR_HAVE_MOTION_SENSOR
                MotionSensorConfig_t motion;
#endif
#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
                AmbientLightSensorConfig_t ambient;
#endif
#if IOT_SENSOR_HAVE_BME280
                BME280Sensor_t bme280;
#endif

            };

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
