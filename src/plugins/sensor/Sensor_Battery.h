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

#ifndef IOT_SENSOR_BATTERY_CHARGING_COMPLETE
// #define IOT_SENSOR_BATTERY_CHARGING_COMPLETE                    digitalRead(5)
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
// see Sensor_Battery::calcLipoCapacity(voltage)
// set to -1 to disable
#ifndef IOT_SENSOR_BATTERY_LEVEL_FUNCTION
#define IOT_SENSOR_BATTERY_LEVEL_FUNCTION(voltage)              Sensor_Battery::calcLipoCapacity(voltage)
#endif

class Sensor_Battery : public MQTTSensor {
public:
    enum class StateType {
        OFF,
        RUNNING,
        RUNNING_ON_BATTERY,
        RUNNING_ON_EXTERNAL_POWER,
        STANDBY
    };

    enum class ChargingType {
        NOT_AVAILABLE,
        NONE,
        CHARGING,
        COMPLETE
    };

    using ConfigType = Plugins::Sensor::BatteryConfig_t;

    class Status {
    public:
        Status() : _state(StateType::RUNNING), _charging(ChargingType::NOT_AVAILABLE), _updateTime(0) {}

        bool isValid() const {
            return (_updateTime != 0) && (get_time_diff(_updateTime, millis()) < 1000);
        }

        void readSensor(Sensor_Battery &sensor);

        float getVoltage() const {
            return _voltage;
        }

        uint8_t getLevel() const {
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
            return IOT_SENSOR_BATTERY_LEVEL_FUNCTION(_voltage);
#endif
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
        StateType _state;
        ChargingType _charging;
        float _voltage;
        uint8_t _battery_level;
        uint32_t _updateTime;
    };

    enum class TopicType {
        VOLTAGE,
        LEVEL,
        CHARGING,
        POWER,
    };

    Sensor_Battery(const JsonString &name);
    virtual ~Sensor_Battery();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUIRoot &webUI, WebUIRow **row) override;
    virtual void getStatus(Print &output) override;
    virtual MQTTSensorSensorType getType() const override {
        return MQTTSensorSensorType::BATTERY;
    }
    virtual bool getSensorData(String &name, StringVector &values) override;

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;

    virtual void reconfigure(PGM_P source) override;

    Status readSensor();

    static constexpr float kRegressMin = 3.490;
    static constexpr float kRegressMax = 4.120;
    static constexpr float kLipoMinV = 3.0; // voltage that represents 0-1% capacity
    static constexpr float kLipoMaxV = 4.15; // 100% charge

    // calculate capacity in %
    // discharge = 0.1C by default. can be used to adjust the discharge curve or adjust to current load
    static uint8_t calcLipoCapacity(float voltage, uint8_t cells = 1, float discharge = 0.1);

private:
    friend Status;

    String _getId(TopicType type);
    String _getTopic(TopicType type);
    // bool _isCharging() const;
    // bool _isOnExternalPower() const;

    JsonString _name;
    ConfigType _config;
    Event::Timer _timer;
    Status _status;
};

#endif
