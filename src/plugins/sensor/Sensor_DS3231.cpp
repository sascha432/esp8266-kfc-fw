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
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch (num) {
    case 0:
        if (discovery->create(this, FSPGM(ds3231_id_temp), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(FSPGM(ds3231_id_temp)));
            discovery->addUnitOfMeasurement(FSPGM(UTF8_degreeC));
            discovery->addName(F("DS3231 Temperature"));
            discovery->addObjectId(baseTopic + F("ds3231_temp"));
        }
        break;
    case 1:
        if (discovery->create(this, FSPGM(ds3231_id_time), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(FSPGM(ds3231_id_time)));
            discovery->addName(F("DS3231 Time"));
            discovery->addObjectId(baseTopic + F("ds3231_time"));
        }
        break;
    case 2:
        if (discovery->create(this, FSPGM(ds3231_id_lost_power), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(FSPGM(ds3231_id_lost_power)));
            discovery->addName(F("DS3231 Status"));
            discovery->addObjectId(baseTopic + F("ds3231_status"));
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
    auto data = _readSensor();
    PrintString timeStr;
    if (!std::isnormal(data.temp)) {
        timeStr = '-';
    }
    else {
        WebUINS::UnnamedTrimmedFormattedFloat(data.temp, F("%.2f")).printTo(timeStr);
    }
    timeStr += F("<span class=\"unit\">");
    timeStr += FSPGM(UTF8_degreeC);
    timeStr += F("</span><br>");
    auto tmp = _getTimeStr(data);
    timeStr += tmp;

    array.append(
        WebUINS::Values(FSPGM(ds3231_id_time), timeStr, tmp.indexOf('\n') != -1)
    );
}

void Sensor_DS3231::createWebUI(WebUINS::Root &webUI)
{
    webUI.appendToLastRow(WebUINS::Row(
        WebUINS::Sensor(FSPGM(ds3231_id_time), _name, F("")).setConfig(_renderConfig)
    ));
}

void Sensor_DS3231::publishState()
{
    if (isConnected()) {
        auto data = _readSensor();
        publish(MQTT::Client::formatTopic(FSPGM(ds3231_id_temp)), true, String(data.temp, 2));
        publish(MQTT::Client::formatTopic(FSPGM(ds3231_id_time)), true, String((uint32_t)data.time));
        publish(MQTT::Client::formatTopic(FSPGM(ds3231_id_lost_power)), true, String(data.lostPower));
    }
}

void Sensor_DS3231::getStatus(Print &output)
{
    output.printf_P(PSTR("DS3231 @ I2C address 0x68" HTML_S(br)));
}

bool Sensor_DS3231::getSensorData(String &name, StringVector &values)
{
    auto data = _readSensor();
    if (!data.time) {
        return false;
    }
    auto tm = gmtime(&data.time);
    char buf[32];
    strftime_P(buf, sizeof(buf), PSTR("RTC %Y-%m-%d %H:%M:%S"), tm);
    name = F("DS3231");
    values.emplace_back(buf);
    values.emplace_back(PrintString(F("%.2f %s"), data.temp, SPGM(UTF8_degreeC)));
    values.emplace_back(PrintString(F("Lost Power: %s"), data.lostPower ? SPGM(Yes) : SPGM(No)));
    return true;
}

Sensor_DS3231::SensorData Sensor_DS3231::_readSensor()
{
    if (!rtc.begin()) {
        delay(5);
        if (!rtc.begin()) {
            return SensorData({NAN, 0, -1});
        }
    }
    return SensorData({rtc.getTemperature(), rtc.now().unixtime(), rtc.lostPower()});
}

String Sensor_DS3231::_getTimeStr(SensorData &data)
{
    String str;
    if (data.time) {
        auto tm = gmtime(&data.time);
        char buf[16];
        strftime_P(buf, sizeof(buf), PSTR("%Y-%m-%d"), tm);
        str = buf;
        str += F(" <span id=\"system_time\">");
        strftime_P(buf, sizeof(buf), PSTR("%H:%M:%S"), tm);
        str += buf;
        str += F("</span><br>\nLost Power: ");
        str += data.lostPower ? FSPGM(Yes) : FSPGM(No);
    }
    else {
        str = F("N/A");
    }
    return str;
}

#endif
