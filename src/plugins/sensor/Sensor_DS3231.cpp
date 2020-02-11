/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_DS3231

#include <Timezone.h>
#include "Sensor_DS3231.h"
#include "progmem_data.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if RTC_DEVICE_DS3231
extern RTC_DS3231 rtc;
#endif

PROGMEM_STRING_DEF(ds3231_id_temp, "ds3231_temp");
PROGMEM_STRING_DEF(ds3231_id_time, "ds3231_time");
PROGMEM_STRING_DEF(ds3231_id_lost_power, "ds3231_lost_power");

Sensor_DS3231::Sensor_DS3231(const JsonString &name) : MQTTSensor(), _name(name), _wire(&config.initTwoWire())
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_DS3231(): component=%p\n"), this);
#endif
    registerClient(this);
}

void Sensor_DS3231::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), FSPGM(ds3231_id_temp)));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), FSPGM(ds3231_id_time)));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), FSPGM(ds3231_id_lost_power)));
    discovery->finalize();
    vector.emplace_back(discovery);
}

uint8_t Sensor_DS3231::getAutoDiscoveryCount() const
{
    return 3;
}

void Sensor_DS3231::getValues(JsonArray &array)
{
    _debug_printf_P(PSTR("Sensor_DS3231::getValues()\n"));

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

void Sensor_DS3231::createWebUI(WebUI &webUI, WebUIRow **row)
{
    _debug_printf_P(PSTR("Sensor_DS3231::createWebUI()\n"));
    (*row)->addSensor(FSPGM(ds3231_id_temp), _name, F("\u00b0C"));
    (*row)->addSensor(FSPGM(ds3231_id_time), F("Time"), F(""));
}

void Sensor_DS3231::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), FSPGM(ds3231_id_temp)), _qos, 1, String(_readSensorTemp(), 2));
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), FSPGM(ds3231_id_time)), _qos, 1, String((uint32_t)_readSensorTime()));
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), FSPGM(ds3231_id_lost_power)), _qos, 1, String(_readSensorLostPower()));
    }
}

void Sensor_DS3231::getStatus(PrintHtmlEntitiesString &output)
{
    output.printf_P(PSTR("DS3231 @ I2C address 0x68" HTML_S(br)));
}

MQTTSensorSensorType Sensor_DS3231::getType() const
{
    return MQTTSensorSensorType::DS3231;
}

float Sensor_DS3231::_readSensorTemp()
{
    _debug_printf_P(PSTR("Sensor_DS3231::_readSensorTemp()\n"));
    if (!rtc.begin()) {
        return NAN;
    }
    return rtc.getTemperature();
}

time_t Sensor_DS3231::_readSensorTime()
{
    _debug_printf_P(PSTR("Sensor_DS3231::_readSensorTime()\n"));
    if (!rtc.begin()) {
        return 0;
    }
    return rtc.now().unixtime();
}

int8_t Sensor_DS3231::_readSensorLostPower()
{
    return 0;
    _debug_printf_P(PSTR("Sensor_DS3231::_readSensorLostPower()\n"));
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
        char buf[32];
        strftime_P(buf, sizeof(buf), PSTR("%Y-%m-%d\n%H:%M:%S"), tm);
        str = buf;
        str += F("\nLost Power: ");
        str += _readSensorLostPower() ? FSPGM(Yes) : FSPGM(No);
    }
    else {
        str = F("N/A");
    }
    return str;
}

#endif
