/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_BATTERY

#include "Sensor_Battery.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_Battery::Sensor_Battery(const JsonString &name) : MQTTSensor(), _name(name) {
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_Battery(): component=%p\n"), this);
#endif
    registerClient(this);
    reconfigure();
}

void Sensor_Battery::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(_getTopic(LEVEL));
    discovery->addUnitOfMeasurement(F("V"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(_getTopic(STATE));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
#endif
}

uint8_t Sensor_Battery::getAutoDiscoveryCount() const {
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    return 2;
#else
    return 1;
#endif
}

void Sensor_Battery::getValues(JsonArray &array) {
    _debug_printf_P(PSTR("Sensor_Battery::getValues()\n"));

    auto obj = &array.addObject(3);
    obj->add(JJ(id), _getId(LEVEL));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(_readSensor(), 2));

#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(STATE));
    obj->add(JJ(state), true);
    obj->add(JJ(value), digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION));
#endif
}

void Sensor_Battery::createWebUI(WebUI &webUI, WebUIRow **row) {
    _debug_printf_P(PSTR("Sensor_Battery::createWebUI()\n"));

    (*row)->addSensor(_getId(LEVEL), _name, F("V"));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    (*row)->addSensor(_getId(STATE), F("Charging"), F(""));
//TODO
#endif
}

void Sensor_Battery::publishState(MQTTClient *client)
{
    if (client) {
        client->publish(_getTopic(LEVEL), _qos, 1, String(_readSensor(), 2));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
        client->publish(_getTopic(STATE), _qos, 1, String(_readSensor(), 2));
#endif
    }
}

void Sensor_Battery::getStatus(PrintHtmlEntitiesString &output)
{
    output.printf_P(PSTR("Supply Voltage Indicator"));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    output.printf_P(", charging: %s"), digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION) ? F("Yes") : F("No"));
#endif
    output.printf_P(PSTR(", calibration %f" HTML_S(br)),  _calibration);
}

void Sensor_Battery::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    auto *sensor = &config._H_W_GET(Config().sensor); // must be a pointer
    form.add<float>(F("battery_calibration"), &sensor->battery.calibration)->setFormUI(new FormUI(FormUI::TEXT, F("Supply Voltage/Battery Calibration")));
}

void Sensor_Battery::reconfigure()
{
    _calibration = config._H_GET(Config().sensor).battery.calibration;
    _debug_printf_P(PSTR("Sensor_Battery::reconfigure(): calibration=%f\n"), _calibration);
}


float Sensor_Battery::readSensor(SensorDataEx_t *data)
{
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == BATTERY) {
            return reinterpret_cast<Sensor_Battery *>(sensor)->_readSensor(data);
        }
    }
    return -1;
}

float Sensor_Battery::_readSensor(SensorDataEx_t *data) {
    double value = 0;
    uint8_t i = 0;
    for(i = 0; i < 3; i++) {
        value += analogRead(A0);
        delay(1);
    }
    if (data) {
        data->adcReadCount = i;
        data->adcSum = value;
    }
    double adcVoltage = value / (i * 1024.0);
    return (((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 + IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1)) / IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1) * adcVoltage * _calibration;
}

String Sensor_Battery::_getId(BatteryIdEnum_t type) {
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    switch(type) {
        case LEVEL:
        case default:
            return F("battery_level");
        case STATE:
            return F("battery_state");
    }
#else
    return F("supply_voltage");
#endif
}

String Sensor_Battery::_getTopic(BatteryIdEnum_t type) {
    return MQTTClient::formatTopic(-1, F("/%s/"), _getId(type).c_str());
}

#endif
