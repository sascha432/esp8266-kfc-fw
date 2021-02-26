/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_DS3231

#include "Sensor_DS3231.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>

#if RTC_DEVICE_DS3231
extern RTC_DS3231 rtc;
#endif

PROGMEM_STRING_DEF(ds3231_id_temp, "ds3231_temp");
PROGMEM_STRING_DEF(ds3231_id_time, "ds3231_time");
PROGMEM_STRING_DEF(ds3231_id_lost_power, "ds3231_lost_power");

Sensor_DS3231::Sensor_DS3231(const JsonString &name) : MQTTSensor(), _name(name), _wire(&config.initTwoWire())
{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_DS3231::~Sensor_DS3231()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTTComponent::MQTTAutoDiscoveryPtr Sensor_DS3231::nextAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0:
            discovery->create(this, FSPGM(ds3231_id_temp), format);
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(ds3231_id_temp)));
            discovery->addUnitOfMeasurement(FSPGM(degree_Celsius_unicode));
            break;
        case 1:
            discovery->create(this, FSPGM(ds3231_id_time), format);
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(ds3231_id_time)));
            break;
        case 2:
            discovery->create(this, FSPGM(ds3231_id_lost_power), format);
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(ds3231_id_lost_power)));
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_DS3231::getAutoDiscoveryCount() const
{
    return 3;
}

void Sensor_DS3231::getValues(JsonArray &array, bool timer)
{
    _debug_println();

    auto obj = &array.addObject(3);
    auto temp = _readSensorTemp();
    obj->add(JJ(id), FSPGM(ds3231_id_temp));
    obj->add(JJ(state), !isnan(temp));
    obj->add(JJ(value), JsonNumber(temp, 2));

    obj = &array.addObject(3);
    String timeStr = _getTimeStr();
    obj->add(JJ(id), FSPGM(ds3231_id_time));
    obj->add(JJ(state), timeStr.indexOf('\n') != -1);
    obj->add(JJ(value), timeStr);
}

void Sensor_DS3231::createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    _debug_println();
    (*row)->addSensor(FSPGM(ds3231_id_temp), _name, JsonString());
    auto &clock = (*row)->addSensor(FSPGM(ds3231_id_time), F("RTC Clock"), JsonString());
    clock.add(JJ(head), F("h4"));
}

void Sensor_DS3231::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(FSPGM(ds3231_id_temp)), true, String(_readSensorTemp(), 2));
        client->publish(MQTTClient::formatTopic(FSPGM(ds3231_id_time)), true, String((uint32_t)_readSensorTime()));
        client->publish(MQTTClient::formatTopic(FSPGM(ds3231_id_lost_power)), true, String(_readSensorLostPower()));
    }
}

void Sensor_DS3231::getStatus(Print &output)
{
    output.printf_P(PSTR("DS3231 @ I2C address 0x68" HTML_S(br)));
}

MQTTSensorSensorType Sensor_DS3231::getType() const
{
    return MQTTSensorSensorType::DS3231;
}

bool Sensor_DS3231::getSensorData(String &name, StringVector &values)
{
    auto now = _readSensorTime();
    if (!now) {
        return false;
    }
    auto tm = gmtime(&now);
    char buf[32];
    strftime_P(buf, sizeof(buf), PSTR("RTC %Y-%m-%d %H:%M:%S"), tm);
    name = F("DS3231");
    values.emplace_back(buf);
    values.emplace_back(PrintString(F("%.2f &deg;C"), _readSensorTemp()));
    values.emplace_back(PrintString(F("Lost Power: %s"), _readSensorLostPower() ? SPGM(Yes) : SPGM(No)));
    return true;
}

float Sensor_DS3231::_readSensorTemp()
{
    _debug_println();
    if (!rtc.begin()) {
        return NAN;
    }
    return rtc.getTemperature();
}

time_t Sensor_DS3231::_readSensorTime()
{
    _debug_println();
    if (!rtc.begin()) {
        return 0;
    }
    return rtc.now().unixtime();
}

int8_t Sensor_DS3231::_readSensorLostPower()
{
    // return 0;
    _debug_println();
    if (!rtc.begin()) {
        return -1;
    }
    return rtc.lostPower();
}

String Sensor_DS3231::_getTimeStr()
{
    String str;
    auto now = _readSensorTime();
    if (now) {
        auto tm = gmtime(&now);
        char buf[16];
        strftime_P(buf, sizeof(buf), PSTR("%Y-%m-%d"), tm);
        str = buf;
        str += F(" <span id=\"system_time\">");
        strftime_P(buf, sizeof(buf), PSTR("%H:%M:%S"), tm);
        str += buf;
        str += F("</span><br>\nLost Power: ");
        str += _readSensorLostPower() ? FSPGM(Yes) : FSPGM(No);
    }
    else {
        str = F("N/A");
    }
    return str;
}

#endif
