/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_BATTERY

#include "Sensor_Battery.h"

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
    _topic = MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str());
}

void Sensor_Battery::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(_topic);
    discovery->addUnitOfMeasurement(F("V"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(_topic + F("_ci"));
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
    obj->add(JJ(id), _getId());
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(_readSensor(), 2));

#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId() + F("_ci"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION));
#endif
}

void Sensor_Battery::createWebUI(WebUI &webUI, WebUIRow **row) {
    _debug_printf_P(PSTR("Sensor_Battery::createWebUI()\n"));

    (*row)->addSensor(_getId(), _name, F("V"));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    (*row)->addSensor(_getId() + F("_ci"), F("Charging"), F(""));
//TODO
#endif
}

void Sensor_Battery::publishState(MQTTClient *client) {
    if (client) {
        client->publish(_topic, _qos, 1, String(_readSensor(), 2));
    }
}

void Sensor_Battery::getStatus(PrintHtmlEntitiesString &output) {
    output.printf_P(PSTR("Battery Indicator"));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    output.printf_P(PSTR(HTML_S(br) "Charging: %s"), digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION) ? F("Yes") : F("No"));
#endif
}

Sensor_Battery::SensorEnumType_t Sensor_Battery::getType() const {
    return BATTERY;
}

float Sensor_Battery::_readSensor() {
    uint32_t value = 0;
    for(int i = 0; i < 3; i++) {
        value += analogRead(A0);
        delay(1);
    }
    return value * (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER / 3.0 / 1024.0);
}

String Sensor_Battery::_getId() {
    PrintString id(F("battery"));
    return id;
}

#endif
