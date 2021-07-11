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

Sensor_Motion::~Sensor_Motion()
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

uint8_t Sensor_Motion::getAutoDiscoveryCount() const
{
    return 1;
}

void Sensor_Motion::getValues(WebUINS::Events &array, bool timer)
{
    using namespace WebUINS;
    array.append(
        Values(_getId(), _motion, true)
    );
}

void Sensor_Motion::createWebUI(WebUINS::Root &webUI)
{
    WebUINS::Row row(
        WebUINS::Sensor(_getId(), _name, '')
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
        publish(_getTopic(), true, String(_motion ? 1 : 0));
    }
}

void Sensor_Battery::getStatus(Print &output)
{
    output.printf_P(PSTR(IOT_SENSOR_NAMES_MOTION_SENSOR));
    // output.print(F(""));
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

void Sensor_Motion::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    auto &group = form.addCardGroup(F("smcfg"), F(IOT_SENSOR_NAMES_BATTERY), true);
    group.end();
}

void Sensor_Motion::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig().motion;
}

#endif
