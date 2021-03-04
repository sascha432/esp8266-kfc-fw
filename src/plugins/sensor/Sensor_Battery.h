/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_BATTERY

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include <ReadADC.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

using KFCConfigurationClasses::Plugins;

// convert ADC value to voltage
#ifndef IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1
#define IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1                   68.0
#endif
#ifndef IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2
#define IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2                   300.0
#endif

// Vadc = (Vs * R1) / (R1 + R2)
// Vadc = ADC / 1024

// Vs = ((R2 + R1) * Vadc) / R1

// Vs = ((R2 + R1) * (ADC / 1024)) / R1
// Vs = ((ADC * R2) * (ADC * R1)) / (1024 * R1)


#ifndef IOT_SENSOR_BATTERY_CHARGING_HAVE_FUNCTION
#define IOT_SENSOR_BATTERY_CHARGING_HAVE_FUNCTION               0
#endif

#if IOT_SENSOR_BATTERY_CHARGING_HAVE_FUNCTION
extern bool Sensor_Battery_charging_detection();
#define IOT_SENSOR_BATTERY_CHARGING                                Sensor_Battery_charging_detection()
#endif

#ifndef IOT_SENSOR_BATTERY_CHARGING
// #define IOT_SENSOR_BATTERY_CHARGING                             digitalRead()
#endif

// set pin to detect end of charge cycle.
// mode is active low and pin gets set to INPUT_PULLUP before reading it with interrupts disabled
// GPIO3/RX can be used. the pin should be floating while not charging. pulling GPIO3 low with 10K seems to work
#ifndef IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN
#define IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN                -1
#endif

#ifndef IOT_SENSOR_BATTERY_ON_BATTERY
// #define IOT_SENSOR_BATTERY_ON_BATTERY                           digitalRead(5)
#endif

#ifndef IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
#define IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS                 0
#endif

#ifndef IOT_SENSOR_BATTERY_ON_EXTERNAL
// #define IOT_SENSOR_BATTERY_ON_EXTERNAL                          digitalRead(5)
#endif

#ifndef IOT_SENSOR_BATTERY_ON_STANDBY
// #define IOT_SENSOR_BATTERY_ON_STANDBY                          !digitalRead(5)
#endif

#ifndef IOT_SENSOR_BATTERY_DISPLAY_LEVEL
#define IOT_SENSOR_BATTERY_DISPLAY_LEVEL                        0
#endif

// function that returns an integer of the battery level in %
// see Sensor_Battery::calcLipoCapacity(voltage, number_of_cells, is_charging)
// the functions uses polynomial regression to fit the voltage to the capacity level and probably needs adjustments
// for different batteries and discharge/charging currents
// max. voltage and an offset can be configured at the WebUI
// calibrate the ADC to reach a maximum of 4.26V while charging
#ifndef IOT_SENSOR_BATTERY_LEVEL_FUNCTION
#define IOT_SENSOR_BATTERY_LEVEL_FUNCTION(u, n, c)              Sensor_Battery::calcLipoCapacity(u, n, c)
#endif

// number of cells
#ifndef IOT_SENSOR_BATTERY_NUM_CELLS
#define IOT_SENSOR_BATTERY_NUM_CELLS                            1
#endif

// allows to configure host and port to receive battery sensor data over UDP to create a charge/discharge curve
// raw ADC values are sent multiple times per second
//
// data records
//
// ["<device-name>","<data type>",<unixtime>,<voltage>,<voltage without calibration>,<ADC average value>,<battery level>,<charging indicator>]
// ["KFCDB1DE7","SEN",1613926400.217,4.2507,4.3324,821.5605,100,0]
// ["KFCDB1DE7","SEN",1613931008.875,4.015,4.0922,776.0072,81,0]
//
// ["<device-name>","<data type>",<unixtime>,<ADC value>,<time since last read in milliseconds>]
// ["KFCDB1DE7","ADC",1613924992.606,792,122]
// ["KFCDB1DE7","ADC",1613924992.683,792,76]
// ["KFCDB1DE7","ADC",1613924992.861,792,179]
// ["KFCDB1DE7","ADC",1613924992.936,792,75]

#ifndef IOT_SENSOR_HAVE_BATTERY_RECORDER
#define IOT_SENSOR_HAVE_BATTERY_RECORDER                        0
#endif

class Sensor_Battery : public MQTT::Sensor {
public:
    enum class StateType : uint8_t {
        OFF,
        RUNNING,
        RUNNING_ON_BATTERY,
        RUNNING_ON_EXTERNAL_POWER,
        STANDBY
    };

    enum class ChargingType : uint8_t {
        NOT_AVAILABLE,
        NONE,
        CHARGING,
        COMPLETE
    };

    using ConfigType = Plugins::Sensor::BatteryConfig_t;
    using RegressFunction = std::function<float(float)>;

    class Status {
    public:
        Status() :
            _lockLevelTime(0),
            _state(StateType::RUNNING),
            _charging(ChargingType::NOT_AVAILABLE),
            _chargingBefore(ChargingType::NOT_AVAILABLE),
            _level(100)
        {}

        void updateSensor(Sensor_Battery &sensor);

        float getVoltage() const {
            return _voltage;
        }

        uint8_t getLevel() const {
            return _level;
        }

        String getChargingStatus() const {
            switch(_charging) {
                case ChargingType::NONE:
                    return F("Not Charging");
                case ChargingType::CHARGING:
                    return F("Charging");
                case ChargingType::COMPLETE:
                    return F("Charging Complete");
                default:
                case ChargingType::NOT_AVAILABLE:
                    break;
            }
            return F("N/A");
        }

        const __FlashStringHelper *getPowerStatus() const {
            switch(_state) {
                case StateType::STANDBY:
                    return F("On Standby");
                case StateType::RUNNING_ON_EXTERNAL_POWER:
                    return F("On External Power");
                case StateType::RUNNING_ON_BATTERY:
                    return F("On Battery");
                case StateType::RUNNING:
                    return F("Power On");
                default:
                    break;
            }
            return F("Power Off");
        }

        bool isCharging() const {
            return _charging == ChargingType::CHARGING;
        }

    private:
        uint32_t _lockLevelTime;
        float _voltage;
        StateType _state;
        ChargingType _charging;
        ChargingType _chargingBefore;
        uint8_t _level;
    };

    enum class TopicType {
        VOLTAGE,
        LEVEL,
        CHARGING,
        POWER,
    };

    Sensor_Battery(const JsonString &name);
    virtual ~Sensor_Battery();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(NamedJsonArray &array, bool timer) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUIRoot &webUI, WebUIRow **row) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;

    virtual void reconfigure(PGM_P source) override;

    Status readSensor();

    // calculate capacity in %
    static float calcLipoCapacity(float voltage, uint8_t cells = 1, bool charging = false, float precision = 1.0);

#if AT_MODE_SUPPORTED && (IOT_SENSOR_BATTERY_DISPLAY_LEVEL || IOT_SENSOR_HAVE_BATTERY_RECORDER)
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

private:
    friend Status;

    void _readADC(bool updateSensor);

    String _getId(TopicType type);
    String _getTopic(TopicType type);

    JsonString _name;
    ConfigType _config;
    Event::Timer _timer;
    float _adcValue;
    uint32_t _adcLastUpdateTime;
    uint32_t _timerCounter;
    Status _status;

public:
    static float maxVoltage;
};

#endif
