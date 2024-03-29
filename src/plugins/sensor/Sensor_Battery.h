/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_BATTERY

#include <Arduino_compat.h>
#ifndef DISABLE_TWO_WIRE
#   include <Wire.h>
#endif
#include <vector>
#include <ReadADC.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

using Plugins = KFCConfigurationClasses::PluginsType;

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


// charging detection
#ifndef IOT_SENSOR_BATTERY_CHARGING
// #define IOT_SENSOR_BATTERY_CHARGING                             digitalRead(...)
// #define IOT_SENSOR_BATTERY_CHARGING                             isBatteryCharging()
#endif

// if a function or variable is used, it can be delcared here
#ifndef IOT_SENSOR_BATTERY_CHARGING_DECL
// #define IOT_SENSOR_BATTERY_CHARGING_DECL                        extern bool isBatteryCharging()
#endif

// pin to detect end of charge cycle (using a TP4056)
// the mode is active low and requires a pullup resistor
// if GPIO3/RX is used, the UART is set to TX only before the pin is set to INPUT_PULLUP
// and after the check, the pin is reset to INPUT and UART to FULL mode. this makes it
// possible to use the serial port unless the charging has finished and the pin gets pulled
// low. the pin should be connected to the standby signal (active low) using a 1.5-3.3K resistor
// in series (the internal pullup of the esp8266 is between 30K and 100K) during charge or if
// the TP4056 is not connected, the standby output is floating

#ifndef IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN
#define IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN                -1
#endif

// detection if running on battery
#ifndef IOT_SENSOR_BATTERY_ON_BATTERY
// #define IOT_SENSOR_BATTERY_ON_BATTERY                           digitalRead(5)
#endif

// can be used to declare the function used for IOT_SENSOR_BATTERY_ON_BATTERY
#ifndef IOT_SENSOR_BATTERY_ON_BATTERY_DECL
// #define IOT_SENSOR_BATTERY_ON_BATTERY_DECL                      extern bool onBattery()
#endif

#ifndef IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
#define IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS                 0
#endif

// condition for running on external power
// if a function or variable is used, it can be decalred with IOT_SENSOR_BATTERY_ON_EXTERNAL_DECL
#ifndef IOT_SENSOR_BATTERY_ON_EXTERNAL
// #define IOT_SENSOR_BATTERY_ON_EXTERNAL                          digitalRead(5)
// #define IOT_SENSOR_BATTERY_ON_EXTERNAL                          (onExternalPower)
#endif

#ifndef IOT_SENSOR_BATTERY_ON_EXTERNAL_DECL
// #define IOT_SENSOR_BATTERY_ON_EXTERNAL_DECL                     extern bool onExternalPower;
#endif

#ifndef IOT_SENSOR_BATTERY_ON_STANDBY
// #define IOT_SENSOR_BATTERY_ON_STANDBY                          !digitalRead(5)
#endif

#ifndef IOT_SENSOR_BATTERY_ON_STANDBY_DECL
// #define IOT_SENSOR_BATTERY_ON_STANDBY_DECL                      extern ...;
#endif

#ifndef IOT_SENSOR_BATTERY_DISPLAY_LEVEL
#define IOT_SENSOR_BATTERY_DISPLAY_LEVEL                        0
#endif

// function that returns an integer of the battery level in %
// see Sensor_Battery::calcLipoCapacity(voltage, number_of_cells, is_charging)
// the functions uses polynomial regression to fit the voltage to the capacity level and probably needs
// adjustments for different batteries and discharge/charging currents
// max. voltage and an offset can be configured at the WebUI
// calibrate the ADC to reach a maximum of 4.26V while charging
#ifndef IOT_SENSOR_BATTERY_LEVEL_FUNCTION
#define IOT_SENSOR_BATTERY_LEVEL_FUNCTION(u, n, c)              Sensor_Battery::calcLipoCapacity(u, n, c)
#endif

// can be used to declare a function used in IOT_SENSOR_BATTERY_LEVEL_FUNCTION
#ifndef IOT_SENSOR_BATTERY_LEVEL_FUNCTION_DECL
// #define IOT_SENSOR_BATTERY_LEVEL_FUNCTION_DECL                  extern myOwnCalcLipoCapacity(float voltage, int cells, bool isCharging)
#endif

// number of cells
#ifndef IOT_SENSOR_BATTERY_NUM_CELLS
#define IOT_SENSOR_BATTERY_NUM_CELLS                            1
#endif

#ifdef IOT_SENSOR_BATTERY_ON_BATTERY_DECL
IOT_SENSOR_BATTERY_ON_BATTERY_DECL;
#endif

#ifdef IOT_SENSOR_BATTERY_ON_EXTERNAL_DECL
IOT_SENSOR_BATTERY_ON_EXTERNAL_DECL;
#endif

#ifdef IOT_SENSOR_BATTERY_ON_STANDBY_DECL
IOT_SENSOR_BATTERY_ON_STANDBY_DECL;
#endif

#ifdef IOT_SENSOR_BATTERY_CHARGING_DECL
IOT_SENSOR_BATTERY_CHARGING_DECL;
#endif

#ifdef IOT_SENSOR_BATTERY_LEVEL_FUNCTION_DECL
IOT_SENSOR_BATTERY_LEVEL_FUNCTION_DECL;
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

    using ConfigType = KFCConfigurationClasses::Plugins::SensorConfigNS::BatteryConfigType;
    using RegressFunction = std::function<float(float)>;

    static constexpr uint8_t kADCNumReads = 16; // number of reads for average value
    static constexpr uint32_t kReadInterval = 1000; // millis

    class Status {
    public:
        Status() :
            _voltage(0),
            _state(StateType::RUNNING),
            _charging(ChargingType::NOT_AVAILABLE),
            _chargingBefore(ChargingType::NOT_AVAILABLE),
            _level(0)
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

    Sensor_Battery(const String &name);
    virtual ~Sensor_Battery();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;

    virtual void reconfigure(PGM_P source) override;

    Status readSensor();
    void readADC(uint8_t num = kADCNumReads);

    // calculate capacity in %
    static float calcLipoCapacity(float voltage, uint8_t cells = 1, bool charging = false);

    #if AT_MODE_SUPPORTED && IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        #if AT_MODE_HELP_SUPPORTED
            virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const;
        #endif
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

private:
    friend Status;

    void _readADC(uint8_t num = kADCNumReads);

    const __FlashStringHelper *_getId(TopicType type);
    String _getTopic(TopicType type);

    String _name;
    ConfigType _config;
    Event::Timer _timer;
    float _adcValue;
    Status _status;

public:
    static float maxVoltage;
};

inline Sensor_Battery::Status Sensor_Battery::readSensor()
{
    return _status;
}

inline void Sensor_Battery::readADC(uint8_t num)
{
    _readADC(num);
}

inline const __FlashStringHelper *Sensor_Battery::_getId(TopicType type)
{
    switch(type) {
        case TopicType::VOLTAGE:
            return F("voltage");
        case TopicType::POWER:
            return F("power");
        case TopicType::CHARGING:
            return F("charging");
        case TopicType::LEVEL:
        default:
            return F("level");
    }
}

inline String Sensor_Battery::_getTopic(TopicType type)
{
    return MQTT::Client::formatTopic(_getId(type));
}

#endif
