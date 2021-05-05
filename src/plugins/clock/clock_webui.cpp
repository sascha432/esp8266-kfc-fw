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

void ClockPlugin::getValues(WebUINS::Events &array)
{
    array.append(WebUINS::Values(F("btn_animation"), _config.animation));

    IF_IOT_CLOCK(
        array.append(WebUINS::Values(F("btn_colon"), (_config.blink_colon_speed < kMinBlinkColonSpeed) ? 0 : (_config.blink_colon_speed < 750 ? 2 : 1), _tempBrightness != -1));
    )

    array.append(
        WebUINS::Values(F("color"), _getColor(), _config.getAnimation() == AnimationType::FADING || _config.getAnimation() == AnimationType::SOLID),
        WebUINS::Values(FSPGM(brightness), static_cast<uint8_t>(_targetBrightness ? _targetBrightness : _savedBrightness), _tempBrightness != -1)
    );

    if (_tempBrightness == -1) {
        array.append(WebUINS::Values(F("tempp"), F("OVERHEATED")));
    }
    else {
        array.append(WebUINS::Values(F("tempp"), WebUINS::FormattedDouble(100 - _tempBrightness * 100.0, 1)));
    }

    IF_IOT_CLOCK_SAVE_STATE(
        array.append(WebUINS::Values(F("power"), static_cast<uint8_t>(_getEnabledState(), _getEnabledState())));
    )

    IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
        array.append(WebUINS::Values(FSPGM(light_sensor), _getLightSensorWebUIValue()));
    )

    IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(
        array.append(WebUINS::Values(F("pwrlvl"), WebUINS::FormattedDouble(_getPowerLevel(), 2)));
    )
}

void ClockPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u value=%s has_state=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        _resetAlarm();
        auto val = static_cast<uint32_t>(value.toInt());
        IF_IOT_CLOCK(
            if (id == F("btn_colon")) {
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
                IF_IOT_CLOCK_SAVE_STATE(
                    _saveStateDelayed();
                )
            }
            else
        )
        IF_IOT_CLOCK_SAVE_STATE(
            if (id == F("power")) {
                _setState(val);
            }
            else
        )
        if (id == F("btn_animation")) {
            setAnimation(static_cast<AnimationType>(val));
            IF_IOT_CLOCK_SAVE_STATE(
                _saveStateDelayed();
            )
        }
        else if (id == F("color")) {
            setColorAndRefresh(val);
            IF_IOT_CLOCK_SAVE_STATE(
                _saveStateDelayed();
            )
        }
        else if (id == FSPGM(brightness)) {
            setBrightness(std::clamp<uint8_t>(val, 0, kMaxBrightness));
            IF_IOT_CLOCK_SAVE_STATE(
                _saveStateDelayed();
            )
        }
    }
}

#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION

void ClockPlugin::addPowerSensor(WebUINS::Root &webUI, SensorPlugin::SensorType type)
{
#ifdef IOT_SENSOR_HAVE_INA219
    if (type == SensorPlugin::SensorType::INA219) {
        webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(F("pwrlvl"), F("Calculated Power"), 'W')));
#else
    if (type == SensorPlugin::SensorType::SYSTEM_METRICS) {
        webUI.addRow(WebUINS::Row(WebUINS::Sensor(F("pwrlvl"), F("Power"), 'W')));
#endif
    }
}

void ClockPlugin::_updatePowerLevelWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(
            WebUINS::Events(WebUINS::Values(F("pwrlvl"), WebUINS::FormattedDouble(_getPowerLevel(), 2)))
        ));
    }
}

uint8_t ClockPlugin::_calcPowerFunction(uint8_t targetBrightness, uint32_t maxPower_mW)
{
    uint32_t requestedPower_mW;
    uint8_t newBrightness = _calculate_max_brightness_for_power_mW(targetBrightness, maxPower_mW, requestedPower_mW);
    if (_config.enabled) {
        _powerLevelCurrentmW = (targetBrightness == newBrightness) ? requestedPower_mW : maxPower_mW;
        _calcPowerLevel();
    }
    else {
        _powerLevelCurrentmW = 0;
        _powerLevelUpdateTimer = 0;
    }
    return newBrightness;
}

void ClockPlugin::_webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id)
{
    if (server != WebUISocket::getServerSocket()) {
        return;
    }
    // adjust update rate if a client connects to the webui
    if (type == WsClient::ClientCallbackType::ON_START) {
        _powerLevelUpdateRate = (IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE * kPowerLevelUpdateRateMultiplier);
        _calcPowerLevel();
    }
    else if (type == WsClient::ClientCallbackType::ON_END) {
        _powerLevelUpdateRate = (kUpdateMQTTInterval * kPowerLevelUpdateRateMultiplier);
    }
}

void ClockPlugin::_calcPowerLevel()
{
    if (_powerLevelUpdateTimer == 0) {
        _powerLevelUpdateTimer = micros();
        _powerLevelAvg = _powerLevelCurrentmW;
    }
    else {
        auto ms = micros();
        auto diff = _powerLevelUpdateRate / static_cast<float>(get_time_diff(_powerLevelUpdateTimer, ms));
        _powerLevelAvg = ((_powerLevelAvg * diff) + _powerLevelCurrentmW) / (diff + 1.0);
        // __LDBG_printf("currentmW=%u diff=%f avgmW=%.3f", _powerLevelCurrentmW, diff, _powerLevelAvg);
        _powerLevelUpdateTimer = ms;
    }
}

#endif

void ClockPlugin::createWebUI(WebUINS::Root &webUI)
{
    #if IOT_LED_MATRIX
        webUI.addRow(WebUINS::Group(F("LED Matrix"), false));
    #else
        webUI.addRow(WebUINS::Group(F("Clock"), false));
    #endif


    webUI.addRow(WebUINS::Slider(FSPGM(brightness), FSPGM(brightness), 0, kMaxBrightness, true));
    webUI.addRow(WebUINS::RGBSlider(F("color"), F("Color")));

    WebUINS::Row row;
    auto height = F("15rem");

    IF_IOT_CLOCK_SAVE_STATE(
        auto power = WebUINS::Switch(F("power"), F("Power<div class=\"p-1\"></div><span class=\"oi oi-power-standby\">"), true, WebUINS::NamePositionType::TOP);
        power.append(WebUINS::NamedString(J(height), height));
        row.append(power);
    )

    IF_IOT_CLOCK(
        auto colon = WebUINS::ButtonGroup(F("btn_colon"), F("Colon"), F("Solid,Blink slowly,Blink fast"));
        colon.append(WebUINS::NamedString(J(height), height));
        row.append(colon);
    )

    auto animation = WebUINS::ButtonGroup(F("btn_animation"), F("Animation"), Plugins::ClockConfig::ClockConfig_t::getAnimationNames());
    animation.append(WebUINS::NamedString(J(height), height));
    animation.append(WebUINS::NamedUint32(J(row), 3));

    IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
        auto lightSensor = WebUINS::Sensor(FSPGM(light_sensor), F("Ambient Light Sensor"), F("<img src=\"/images/light.svg\" width=\"80\" height=\"80\" style=\"margin-top:-20px;margin-bottom:1rem\">"), WebUINS::SensorRenderType::COLUMN);
        lightSensor.append(WebUINS::NamedString(J(height), height));
        row.append(lightSensor);
    )

    auto tempProtection = WebUINS::Sensor(F("tempp"), F("Temperature Protection"), '%');
    tempProtection.append(WebUINS::NamedString(J(height), height));
    row.append(tempProtection);

    webUI.addRow(row);
}

void ClockPlugin::_broadcastWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUINS::Events events;
        getValues(events);
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(events));
    }
}
