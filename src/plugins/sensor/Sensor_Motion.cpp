/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_MOTION_SENSOR

#include "Sensor_Motion.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_Motion::Sensor_Motion(const String &name) :
    MQTT::Sensor(MQTT::SensorType::MOTION),
    _name(name),
{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_Battery::~Sensor_Motion()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_Motion::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    switch(num) {
        case 0:
            if (!discovery->create(MQTTComponent::ComponentType::BINARY_SENSOR, F("motion"), format)) {
                return discovery;
            }
            discovery->addStateTopic(MQTTClient::formatTopic(F("motion")));
            break;
    }
    return discovery;
}

uint8_t Sensor_Battery::getAutoDiscoveryCount() const
{
    return 1;
}

void Sensor_Battery::getValues(WebUINS::Events &array, bool timer)
{
    using namespace WebUINS;
    array.append(
        Values(_getId(TopicType::VOLTAGE), TrimmedDouble(_status.getVoltage(), _config.precision))
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        , Values(_getId(TopicType::LEVEL), _status.getLevel(), true)
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        , Values(_getId(TopicType::CHARGING), _status.getChargingStatus(), true)
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
        , Values(_getId(TopicType::POWER), _status.getPowerStatus(), true)
        )
#endif
    );
}

void Sensor_Battery::createWebUI(WebUINS::Root &webUI)
{
    WebUINS::Row row(
        WebUINS::Sensor(_getId(TopicType::VOLTAGE), _name, 'V')
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        , WebUINS::Sensor(_getId(TopicType::LEVEL), F("Level"), '%')
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        , WebUINS::Sensor(_getId(TopicType::CHARGING), F("Charging"), F(""))
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
        , WebUINS::Sensor(_getId(TopicType::POWER), F("Status"), F(""))
#endif
    );
    webUI.appendToLastRow(row);
}

void Sensor_Battery::publishState()
{
    // auto status = readSensor();
    // if (!_status.isValid()) {
    //     return;
    // }
    __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (isConnected()) {
        publish(_getTopic(TopicType::VOLTAGE), true, String(_status.getVoltage(), _config.precision));
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        publish(_getTopic(TopicType::LEVEL), true, String(_status.getLevel()));
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        publish(_getTopic(TopicType::CHARGING), true, _status.getChargingStatus());
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
        publish(_getTopic(TopicType::POWER), true, _status.getPowerStatus());
#endif
    }
}

void Sensor_Battery::getStatus(Print &output)
{
    output.printf_P(PSTR(IOT_SENSOR_NAMES_BATTERY));
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    output.print(F(", Battery Level Indicator"));
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
    output.print(F(", Charging Status"));
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    output.print(F(", Power Status"));
#endif
    output.printf_P(PSTR(", calibration %f" HTML_S(br)), _config.calibration);
}

bool Sensor_Battery::getSensorData(String &name, StringVector &values)
{
    auto status = readSensor();

    name = F(IOT_SENSOR_NAMES_BATTERY);
    values.emplace_back(String(status.getVoltage(), _config.precision) + F(" V"));
#ifdef IOT_SENSOR_BATTERY_CHARGING
    values.emplace_back(PrintString(F("Charging: %s"), status.isCharging() ? SPGM(Yes) : SPGM(No)));
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    values.emplace_back(PrintString(F("Status: %s"), status.getPowerStatus()));
#endif
    return true;
}

void Sensor_Battery::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    auto &group = form.addCardGroup(F("spcfg"), F(IOT_SENSOR_NAMES_BATTERY), true);

    form.addPointerTriviallyCopyable<float>(F("sp_uc"), reinterpret_cast<void *>(&cfg.battery.calibration));
    form.addFormUI(F("Calibration"), FormUI::Suffix(PrintString(F("Max. Voltage %.4f"), maxVoltage)));
    cfg.battery.addRangeValidatorFor_calibration(form);

    form.addPointerTriviallyCopyable<float>(F("sp_ofs"), reinterpret_cast<void *>(&cfg.battery.offset));
    form.addFormUI(F("Offset"));
    cfg.battery.addRangeValidatorFor_offset(form);

    form.addObjectGetterSetter(F("sp_pr"),  FormGetterSetter(cfg.battery, precision));
    form.addFormUI(F("Display Precision"));
    cfg.battery.addRangeValidatorFor_precision(form);

#if IOT_SENSOR_HAVE_BATTERY_RECORDER

    form.addObjectGetterSetter(F("sp_ra"), FormGetterSetter(cfg.battery, address));
    form.addFormUI(F("Data Hostname"));
    cfg.battery.addHostnameValidatorFor_address(form)

    form.addObjectGetterSetter(F("sp_rp"), FormGetterSetter(cfg.battery, port));
    form.addFormUI(F("Data Port"));
    cfg.addRangeValidatorFor_port(form);

    auto items = FormUI::List(
        SensorRecordType::NONE, F("Disabled"),
        SensorRecordType::ADC, F("Record ADC values"),
        SensorRecordType::SENSOR, F("Record sensor values"),
        SensorRecordType::BOTH, F("Record ADC and sensor values")
    );

    form.addObjectGetterSetter(F("sp_brt"), FormGetterSetter(cfg.battery, record));
    form.addFormUI(F("Sensor Data"), items);

#endif

    group.end();
}

void Sensor_Battery::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig().battery;
    maxVoltage = 0;
}

#endif

