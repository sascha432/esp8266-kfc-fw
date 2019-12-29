/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_DS3231

#include "Sensor_DS3231.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern RTC_DS3231 rtc;

Sensor_DS3231::Sensor_DS3231(const JsonString &name) : MQTTSensor(), _name(name) {
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_DS3231(): component=%p\n"), this);
#endif
    registerClient(this);
}

void Sensor_DS3231::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), _getId()));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
}

uint8_t Sensor_DS3231::getAutoDiscoveryCount() const {
    return 1;
}

void Sensor_DS3231::getValues(JsonArray &array) {
    _debug_printf_P(PSTR("Sensor_DS3231::getValues()\n"));

    auto &obj = array.addObject(3);
    auto temp = _readSensor();
    obj.add(JJ(id), _getId());
    obj.add(JJ(state), !isnan(temp));
    obj.add(JJ(value), JsonNumber(temp, 2));
}

void Sensor_DS3231::createWebUI(WebUI &webUI, WebUIRow **row) {
    _debug_printf_P(PSTR("Sensor_DS3231::createWebUI()\n"));
    (*row)->addSensor(_getId(), _name, F("\u00b0C"));
}

void Sensor_DS3231::publishState(MQTTClient *client) {
    if (client) {
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), _getId()), _qos, 1, String(_readSensor(), 2));
    }
}

void Sensor_DS3231::getStatus(PrintHtmlEntitiesString &output) {
    output.printf_P(PSTR("DS3231 @ I2C" HTML_S(br)));
}

Sensor_DS3231::SensorEnumType_t Sensor_DS3231::getType() const {
    return DS3231;
}

float Sensor_DS3231::_readSensor()
{
    if (!rtc.begin()) {
        return NAN;
    }
    return rtc.getTemperature();
}

const __FlashStringHelper *Sensor_DS3231::_getId() {
    return F("ds3231");
}

#endif
