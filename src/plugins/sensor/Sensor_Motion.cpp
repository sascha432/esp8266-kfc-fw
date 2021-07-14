/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_MOTION_SENSOR

#include <EventScheduler.h>
#include <WebUISocket.h>
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
    _handler(nullptr),
    _config(Plugins::Sensor::getConfig().motion),
    _motionLastUpdate(0),
    _motionState(0xff),
    _pin(0xff),
    _pinInverted(false)
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
    // array.append(
    //     WebUINS::Values(_getId(), _getStateStr(_motionState))
    // );
}

void Sensor_Motion::createWebUI(WebUINS::Root &webUI)
{
//     WebUINS::Row row(
//         WebUINS::Sensor(_getId(), _name, emptyString)
//     );
//     webUI.appendToLastRow(row);
}

void Sensor_Motion::publishState()
{
    // __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (isConnected()) {
        publish(_getTopic(), true, MQTT::Client::toBoolOnOff(_motionState));
    }
    // if (WebUISocket::hasAuthenticatedClients()) {
    //     WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(
    //         WebUINS::Events(WebUINS::Values(_getId(), _getStateStr(_motionState)))
    //     ));
    // }
}

void Sensor_Motion::getStatus(Print &output)
{
    output.printf_P(PSTR(IOT_SENSOR_NAMES_MOTION_SENSOR HTML_S(br) "Trigger timeout %u minutes"), _config.motion_trigger_timeout);
    #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
        output.print(F(HTML_S(br) "Device auto off "));
        if (_config.motion_auto_off) {
            output.printf_P(PSTR("%u minutes"), _config.motion_auto_off);
        }
        else {
            output.print(F("disabled"));
        }
    #endif
    output.println(F(HTML_S(br)));
}

void Sensor_Motion::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    auto &group = form.addCardGroup(F("mscfg"), F(IOT_SENSOR_NAMES_MOTION_SENSOR), true);

    form.addObjectGetterSetter(F("msto"), FormGetterSetter(cfg.motion, motion_trigger_timeout));
    form.addFormUI(F("Motion Sensor Timeout"), FormUI::Suffix(F("minutes")));
    cfg.motion.addRangeValidatorFor_motion_trigger_timeout(form);

    #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
        form.addObjectGetterSetter(F("msao"), FormGetterSetter(cfg.motion, motion_auto_off));
        form.addFormUI(F("Auto Off Delay Without Motion"), FormUI::Suffix(F("minutes")), FormUI::IntAttribute(F("disabled-value"), 0));
        cfg.motion.addRangeValidatorFor_motion_auto_off(form);
    #endif

    group.end();
}

void Sensor_Motion::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig().motion;
}

void Sensor_Motion::begin(MotionSensorHandler *handler, uint8_t pin, bool pinInverted)
{
    _handler = nullptr;
    _pin = pin;
    _pinInverted = pinInverted;
    _pinMode(_pin, INPUT);
    _timerCallback();
    _handler = handler;
    _timer.add(Event::seconds(1), true, [this](Event::CallbackTimerPtr) {
        _timerCallback();
    });
}

void Sensor_Motion::end()
{
    _timer.remove();
    _handler = nullptr;
}

void Sensor_Motion::_timerCallback()
{
    bool state = _digitalRead(_pin);
    if (_pinInverted) {
        state = !state;
    }
    if (!state && _motionLastUpdate && (millis() - _motionLastUpdate) < (_config.motion_trigger_timeout * 60000UL)) {
        // keep the motion signal on if any motion is detected within motion_trigger_timeout min.
    }
    else if (state != _motionState) {
        // motion detected or timeout
        _motionState = state;
        if (_motionState) {
            __LDBG_printf("motion detected: state=%u last=%.3fs auto_off=%us", _motionState, _motionLastUpdate ? (millis() - _motionLastUpdate) / 1000.0 : 0.0,  (_config.motion_auto_off * 60));

            #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
                if (_handler && _handler->_autoOff) {
                    if (_motionLastUpdate == 0 && _config.motion_auto_off) {
                        if (_handler->eventMotionAutoOff(true)) {
                            Logger_notice(F("Motion detected. Turning device on..."));
                        }
                    }
                }
            #endif

            _motionLastUpdate = millis();
            if (_motionLastUpdate == 0) {
                _motionLastUpdate++;
            }
        }
        else {
            __LDBG_printf("motion sensor timeout");
        }
        if (_handler) {
            _handler->eventMotionDetected(_motionState);
        }
        publishState();
    }
    else if (state && state == _motionState && _motionLastUpdate) {
        // motion detected within timeout
        __LDBG_printf("motion detected (retrigger): state=%u last=%.3fs auto_off=%us", _motionState, _motionLastUpdate ? (millis() - _motionLastUpdate) / 1000.0 : 0.0,  (_config.motion_auto_off * 60));
        _motionLastUpdate = millis();
        if (_motionLastUpdate == 0) {
            _motionLastUpdate++;
        }
    }

    #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
        if (_handler && _handler->_autoOff) {
            if (_motionLastUpdate && _config.motion_auto_off && get_time_diff(_motionLastUpdate, millis()) > (_config.motion_auto_off * 60000UL)) {
                _motionLastUpdate = 0;
                if (_handler->eventMotionAutoOff(true)) {
                    Logger_notice(F("No motion detected. Turning device off..."));
                }
            }
        }
    #endif
}

#endif
