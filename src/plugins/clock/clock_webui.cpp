/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <WebUISocket.h>
#include <stl_ext/algorithm.h>
#include "../src/plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void ClockPlugin::getValues(JsonArray &array)
{
    auto obj = &array.addObject(3);
    obj->add(JJ(id), F("btn_animation"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _config.animation); //static_cast<int>(_config.animation));

    IF_IOT_CLOCK(
        obj = &array.addObject(2);
        obj->add(JJ(id), F("btn_colon"));
        // obj->add(JJ(state), true);
        obj->add(JJ(value), (_config.blink_colon_speed < kMinBlinkColonSpeed) ? 0 : (_config.blink_colon_speed < 750 ? 2 : 1));
    )

    obj = &array.addObject(3);
    obj->add(JJ(id), F("color"));
    #if IOT_LED_MATRIX
        obj->add(JJ(state), _config.getAnimation() != AnimationType::RAINBOW && _config.getAnimation() != AnimationType::FIRE);
    #else
        obj->add(JJ(state), _config.getAnimation() != AnimationType::RAINBOW);
    #endif
    obj->add(JJ(value), _getColor());

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(brightness));
    obj->add(JJ(state), _targetBrightness != 0);
    obj->add(JJ(value), static_cast<uint32_t>(_targetBrightness ? _targetBrightness : _savedBrightness));

    obj = &array.addObject(2);
    obj->add(JJ(id), F("tempp"));
    if (_tempBrightness == -1) {
        obj->add(JJ(value), F("OVERHEATED"));
    }
    else {
        obj->add(JJ(value), JsonNumber(100 - _tempBrightness * 100.0, 1));
    }

    IF_IOT_CLOCK_SAVE_STATE(
        obj = &array.addObject(3);
        obj->add(JJ(id), F("power"));
        obj->add(JJ(state), _config.enabled && _targetBrightness);
        obj->add(JJ(value), _config.enabled && _targetBrightness);
    )

    IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
        obj = &array.addObject(3);
        obj->add(JJ(id), FSPGM(light_sensor));
        obj->add(JJ(state), true);
        obj->add(JJ(value), _getLightSensorWebUIValue());
    )

    IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(
        obj = &array.addObject(3);
        obj->add(JJ(id), F("pwrlvl"));
        obj->add(JJ(state), true);
        // obj->add(JJ(value), JsonNumber(get_power_level_mW() / 1000.0, 2));
    )
}

void ClockPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u value=%s has_state=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        _resetAlarm();
        auto val = (uint32_t)value.toInt();
        IF_IOT_CLOCK(
            if (String_equals(id, PSTR("btn_colon"))) {
                switch(val) {
                    case 0:
                        setBlinkColon(0);
                        break;
                    case 1:
                        setBlinkColon(1000);
                        break;
                    case 2:
                        setBlinkColon(500);
                        break;
                }
                _saveStateDelayed();
            }
            else
        )
        IF_IOT_CLOCK_SAVE_STATE(
            if (String_equals(id, PSTR("power"))) {
                _setState(val);
            }
            else
        )
        if (String_equals(id, PSTR("btn_animation"))) {
            setAnimation(static_cast<AnimationType>(val));
            _saveStateDelayed();
        }
        else if (String_equals(id, PSTR("color"))) {
            setColorAndRefresh(val);
            _saveStateDelayed();
        }
        else if (String_equals(id, SPGM(brightness))) {
            setBrightness(std::clamp<uint8_t>(val, 0, kMaxBrightness));
            _saveStateDelayed();
        }
    }
}

#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION

void ClockPlugin::addPowerSensor(WebUIRoot &webUI, WebUIRow **row, SensorPlugin::SensorType type)
{
    if (type == SensorPlugin::SensorType::SYSTEM_METRICS) {
        (*row)->addSensor(F("pwrlvl"), F("Power"), 'W');
    }
}

void ClockPlugin::_updatePowerLevelWebUI()
{
    if (WsWebUISocket::hasClients(WsWebUISocket::getWsWebUI())) {
        JsonUnnamedObject json(2);
        json.add(JJ(type), JJ(ue));
        auto &events = json.addArray(JJ(events), 1);
        auto &obj = events.addObject(3);
        obj.add(JJ(id), F("pwrlvl"));
        obj.add(JJ(state), true);
        // obj.add(JJ(value), JsonNumber(get_power_level_mW() / 1000.0, 2));
        WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
    }
}

void ClockPlugin::_powerLevelCallback(uint32_t total_mW, uint32_t requested_mW, uint32_t max_mW, uint8_t target_brightness, uint8_t recommended_brightness)
{
    __LDBG_printf("mW=%u reqmW=%u maxmW=%u brightness=%u recommended=%u currentmW=%u", total_mW, requested_mW, max_mW, target_brightness, recommended_brightness, _powerLevelCurrentmW);
    if (_config.enabled) {
        _calcPowerLevel();
    }
    else {
        _powerLevelCurrentmW = 0;
        _powerLevelUpdateTimer = 0;
    }
}

void ClockPlugin::_webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id)
{
    if (server != WsWebUISocket::getWsWebUI()) {
        return;
    }
    // adjust update rate if a client connects to the webui
    if (type == WsClient::ClientCallbackType::ON_START) {
        _powerLevelUpdateRate = (IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE * 1000);
        _calcPowerLevel();
    }
    else if (type == WsClient::ClientCallbackType::ON_END) {
        _powerLevelUpdateRate = (kUpdateMQTTInterval * 1000);
    }
}

void ClockPlugin::_calcPowerLevel()
{
    if (_powerLevelUpdateTimer == 0) {
        _powerLevelUpdateTimer = millis();
        _powerLevelAvg = _powerLevelCurrentmW;
        __LDBG_printf("currentmW=%u reset", _powerLevelCurrentmW);
    }
    else {
        auto ms = millis();
        auto diff = _powerLevelUpdateRate / get_time_diff(_powerLevelUpdateTimer, ms);
        _powerLevelAvg = ((_powerLevelAvg * diff) + _powerLevelCurrentmW) / (diff + 1.0);
        __LDBG_printf("currentmW=%u diff=%u avgmW=%.3f", _powerLevelCurrentmW, diff, _powerLevelAvg);
        _powerLevelUpdateTimer = ms;
    }
}

float ClockPlugin::_getPowerLevelmW()
{
    return _powerLevelAvg;
}

#endif

void ClockPlugin::createWebUI(WebUIRoot &webUI)
{
    auto row = &webUI.addRow();
    #if IOT_LED_MATRIX
        row->addGroup(F("LED Matrix"), false);
    #else
        row->addGroup(F("Clock"), false);
    #endif

    row = &webUI.addRow();
    row->addSlider(FSPGM(brightness), FSPGM(brightness), 0, kMaxBrightness, true);

    row = &webUI.addRow();
    row->addRGBSlider(F("color"), F("Color"));

    row = &webUI.addRow();
    auto height = F("15rem");

    IF_IOT_CLOCK_SAVE_STATE(
        row->addSwitch(F("power"), F("Power<div class=\"p-1\"></div><span class=\"oi oi-power-standby\">"), true, WebUIRow::NamePositionType::TOP).add(JJ(height), height);
    )

    IF_IOT_CLOCK(
        row->addButtonGroup(F("btn_colon"), F("Colon"), F("Solid,Blink slowly,Blink fast")).add(JJ(height), height);
    )

    auto &col = row->addButtonGroup(F("btn_animation"), F("Animation"), Plugins::ClockConfig::ClockConfig_t::getAnimationNames());
    col.add(JJ(height), height);
    IF_IOT_LED_MATRIX(
        col.add(JJ(row), 3);
    )

    IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
       row->addSensor(FSPGM(light_sensor), F("Ambient Light Sensor"), F("<img src=\"/images/light.svg\" width=\"80\" height=\"80\" style=\"margin-top:-20px;margin-bottom:1rem\">"), WebUIComponent::SensorRenderType::COLUMN).add(JJ(height), height);
    )

    row->addSensor(F("tempp"), F("Temperature Protection"), '%').add(JJ(height), height);
}

void ClockPlugin::_broadcastWebUI()
{
    if (WsWebUISocket::hasClients(WsWebUISocket::getWsWebUI())) {
        JsonUnnamedObject json(2);
        json.add(JJ(type), JJ(ue));
        getValues(json.addArray(JJ(events)));
        WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
    }
}
