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
    _motionState(false),
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
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
    switch(num) {
        case 0:
            if (!discovery->create(MQTTComponent::ComponentType::BINARY_SENSOR, _getId(), format)) {
                return discovery;
            }
            auto baseTopic = MQTT::Client::getBaseTopicPrefix();
            discovery->addStateTopic(_getTopic());
            discovery->addDeviceClass(F("motion"));
            discovery->addName(F("Occupancy"));
            discovery->addObjectId(baseTopic + F("occupancy"));
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
    auto str = PrintString(F("<div class=\"pt-3\">%s</div>"), _getStateStr(_motionState));
    array.append(WebUINS::Values(_getId(), str));
}

void Sensor_Motion::createWebUI(WebUINS::Root &webUI)
{
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(), _name, emptyString).setConfig(_renderConfig)));
}

void Sensor_Motion::publishState()
{
    // __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (isConnected()) {
        publish(_getTopic(), true, MQTT::Client::toBoolOnOff(_motionState));
    }
}

void Sensor_Motion::_publishWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(
            WebUINS::Events(WebUINS::Values(_getId(), _getStateStr(_motionState)))
        ));
    }
}

void Sensor_Motion::getStatus(Print &output)
{
    output.printf_P(PSTR(IOT_SENSOR_NAMES_MOTION_SENSOR HTML_S(br) "Trigger timeout %u minutes"), _config.motion_trigger_timeout);
    #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
        output.print(F(HTML_S(br) "Device auto off "));
        if (_config.motion_auto_off) {
            output.printf_P(PSTR("after %u minutes"), _config.motion_auto_off);
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
    _reset();
}

void Sensor_Motion::begin(MotionSensorHandler *handler, uint8_t pin, bool pinInverted)
{
    _handler = handler;
    _pin = pin;
    _pinInverted = pinInverted;
    pinMode(_pin, INPUT);
    _reset();
}

void Sensor_Motion::end()
{
    _reset(true);
    _handler = nullptr;
}

void Sensor_Motion::_reset(bool shutdown)
{
    _motionState = false;
    _Timer(_sensorTimeout).remove();
    _Timer(_timer).remove();
    #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
        if (_handler) {
            _Timer(_handler->_autoOffTimeout).remove();
        }
    #endif
    if (!shutdown) {
        _Timer(_timer).add(kPollPinInterval, true, [this](Event::CallbackTimerPtr) {
            _timerCallback();
        });
    }
}

void Sensor_Motion::_timerCallback()
{
    bool state = digitalRead(_pin);
    if (_pinInverted) {
        state = !state;
    }

    if (state) {
        // reset timeout
        _Timer(_sensorTimeout).add(Event::minutes(_config.motion_trigger_timeout), false, [this](Event::CallbackTimerPtr) {
            __LDBG_printf("motion timeout event: %umin", _config.motion_trigger_timeout);
            _motionState = false;
            publishState();
            _publishWebUI();

            if (_handler) {
                _handler->eventMotionDetected(false);
                #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
                    // start auto off timeout
                    if (_config.motion_auto_off && _handler->_getMotionAutoOff() == false) {
                        _Timer(_handler->_autoOffTimeout).add(Event::minutes(_config.motion_auto_off), false, [this](Event::CallbackTimerPtr) {
                            __LDBG_printf("motion auto off event: %umin", _config.motion_auto_off);
                            _handler->_setMotionAutoOff(true);
                            if (_handler->eventMotionAutoOff(true)) {
                                Logger_notice(F("No motion detected. Turning device off..."));
                            }
                        });
                    }
                #endif
            }
        });
        // remove auto off timeout
        #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
            if (_handler && _config.motion_auto_off) {
                _Timer(_handler->_autoOffTimeout).remove();
            }
        #endif

        // notify handler if state changed
        if (_motionState == false) {
            __LDBG_printf("motion detected event");
            _motionState = true;
            publishState();
            _publishWebUI();

            if (_handler) {
                _handler->eventMotionDetected(true);
                #if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
                    // device has been disabled by the auto off time
                    // send notification
                    if (_config.motion_auto_off && _handler->_getMotionAutoOff() == true) {
                        __LDBG_printf("motion auto on event");
                        _handler->_setMotionAutoOff(false);
                        if (_handler->eventMotionAutoOff(false)) {
                            Logger_notice(F("Motion detected. Turning device on..."));
                        }
                    }
                #endif
            }
        }
    }
}

#endif
