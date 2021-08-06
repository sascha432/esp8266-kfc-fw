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

#if RTC_DEVICE_DS3231
extern RTC_DS3231 rtc;
#endif

Sensor_DS3231::Sensor_DS3231(const String& name) :
    MQTT::Sensor(MQTT::SensorType::DS3231),
    _name(name),
    _wire(&config.initTwoWire())
{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_DS3231::~Sensor_DS3231()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_DS3231::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new AutoDiscovery::Entity();
    switch (num) {
    case 0:
        if (discovery->create(this, FSPGM(ds3231_id_temp), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(FSPGM(ds3231_id_temp)));
            discovery->addUnitOfMeasurement(FSPGM(UTF8_degreeC));
        }
        break;
    case 1:
        if (discovery->create(this, FSPGM(ds3231_id_time), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(FSPGM(ds3231_id_time)));
        }
        break;
    case 2:
        if (discovery->create(this, FSPGM(ds3231_id_lost_power), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(FSPGM(ds3231_id_lost_power)));
        }
        break;
    }
    return discovery;
}

uint8_t Sensor_DS3231::getAutoDiscoveryCount() const
{
    return 3;
}

void Sensor_DS3231::getValues(WebUINS::Events &array, bool timer)
{
    auto temp = _readSensorTemp();
    String timeStr = _getTimeStr();
    array.append(
        WebUINS::Values(FSPGM(ds3231_id_temp), WebUINS::TrimmedDouble(temp, 2), isnan(temp)),
        WebUINS::Values(FSPGM(ds3231_id_time), timeStr, timeStr.indexOf('\n') != -1)
    );
}

void Sensor_DS3231::createWebUI(WebUINS::Root &webUI)
{
    webUI.appendToLastRow(WebUINS::Row(
        WebUINS::Sensor(FSPGM(ds3231_id_time), _name, FSPGM(UTF8_degreeC)).setConfig(_renderConfig),
        WebUINS::Sensor(FSPGM(ds3231_id_temp), F("RTC Clock"), FSPGM(UTF8_degreeC)).setConfig(_renderConfig)
    ));
}

void Sensor_DS3231::publishState()
{
    if (isConnected()) {
        publish(MQTT::Client::formatTopic(FSPGM(ds3231_id_temp)), true, String(_readSensorTemp(), 2));
        publish(MQTT::Client::formatTopic(FSPGM(ds3231_id_time)), true, String((uint32_t)_readSensorTime()));
        publish(MQTT::Client::formatTopic(FSPGM(ds3231_id_lost_power)), true, String(_readSensorLostPower()));
    }
}

void Sensor_DS3231::getStatus(Print &output)
{
    output.printf_P(PSTR("DS3231 @ I2C address 0x68" HTML_S(br)));
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
