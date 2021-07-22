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
    auto enabled = _getEnabledState();

    #if IOT_LED_MATRIX == 0
        array.append(WebUINS::Values(F("colon"), enabled && (_config.blink_colon_speed < kMinBlinkColonSpeed) ? 0 : (_config.blink_colon_speed < 750 ? 500 : 1000), enabled));
    #endif

    array.append(
        WebUINS::Values(F("ani"), static_cast<int>(_config.animation), enabled),
        WebUINS::Values(F("color"), Color(_getColor()).toString(), enabled && _config.hasColorSupport()),
        WebUINS::Values(FSPGM(brightness), static_cast<uint8_t>(enabled ? _targetBrightness : _savedBrightness), true/* changing brightness can be used to turn the LEDs on */)
    );

    if (_tempBrightness == -1) {
        array.append(WebUINS::Values(F("tempp"), _overheatedInfo));
    }
    else {
        if (_tempBrightness == 1.0) {
            array.append(WebUINS::Values(F("tempp"), F("Off")));
        } else {
            array.append(WebUINS::Values(F("tempp"), WebUINS::FormattedDouble(100 - _tempBrightness * 100.0, 1)));
        }
    }

    #if IOT_CLOCK_SAVE_STATE
        array.append(WebUINS::Values(F("power"), static_cast<uint8_t>(enabled), true));
    #endif

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        array.append(WebUINS::Values(F("pwrlvl"), _getPowerLevelStr()));
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        array.append(WebUINS::Values(F("fanspeed"), _fanSpeed, true));
    #endif
}

bool ClockPlugin::getValue(const String &id, String &value, bool &state)
{
    bool result = false;
    state = false;
    if (id == F("animation-1")) {
        state = _config.getAnimation() == AnimationType::RAINBOW;
        //TODO check if the data can be created from the Forms class
        value = PrintString(F("{\"_prefix\":\"#rb_\","
            "\"mode\":%u,\"bpm\":%u,\"hue\":%u,"
            "\"mul\":%.*f,\"incr\":%.*f,\"min\":%.*f,\"max\":%.*f,"
            "\"sp\":%u,\"cf\":\"#%06X\",\"mv\":\"#%06X\","
            "\"cre\":%.*f,\"cgr\":%.*f,\"cbl\":%.*f}"
        ),
            _config.rainbow._get_int_mode(),
            _config.rainbow.bpm,
            _config.rainbow.hue,
            countDecimalPlaces(_config.rainbow.multiplier.value),
            _config.rainbow.multiplier.value,
            countDecimalPlaces(_config.rainbow.multiplier.incr),
            _config.rainbow.multiplier.incr,
            countDecimalPlaces(_config.rainbow.multiplier.min),
            _config.rainbow.multiplier.min,
            countDecimalPlaces(_config.rainbow.multiplier.max),
            _config.rainbow.multiplier.max,
            _config.rainbow.speed,
            _config.rainbow.color.factor.value,
            _config.rainbow.color.min.value,
            countDecimalPlaces(_config.rainbow.color.red_incr),
            _config.rainbow.color.red_incr,
            countDecimalPlaces(_config.rainbow.color.green_incr),
            _config.rainbow.color.green_incr,
            countDecimalPlaces(_config.rainbow.color.blue_incr),
            _config.rainbow.color.blue_incr
        );
        result = true;
    }
    __LDBG_printf("id=%s value=%s state=%u result=%u", id.c_str(), value.c_str(), state, result);
    return result;
}

void ClockPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u value=%s has_state=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        #if IOT_ALARM_PLUGIN_ENABLED
            _resetAlarm();
        #endif
        auto val = static_cast<uint32_t>(value.toInt());
        #if IOT_LED_MATRIX == 0
            if (id == F("colon")) {
                setBlinkColon(val);
                _saveStateDelayed();
            }
            else
        #endif
        #if IOT_CLOCK_SAVE_STATE
            if (id == F("power")) {
                _setState(val);
            }
            else
        #endif
        #if IOT_LED_MATRIX_FAN_CONTROL
            if (id == F("fanspeed")) {
                _setFanSpeed(val);
                _config.fan_speed = _fanSpeed;
                _saveStateDelayed();
                // if (val != _fanSpeed) {
                //     _webUIUpdateFanSpeed();
                // }
            }
            else
        #endif
        if (id == F("ani")) {
            setAnimation(static_cast<AnimationType>(val));
            _saveStateDelayed();
        }
        else if (id == F("animation-1")) {
            StringVector items;
            explode(value.c_str(), ',', items);
            __DBG_printf("animation-1 %s", implode(',', items).c_str());
        }
        else if (id == F("color")) {
            setColorAndRefresh(val);
            _saveStateDelayed();
        }
        else if (id == FSPGM(brightness)) {
            setBrightness(std::clamp<uint8_t>(val, 0, kMaxBrightness));
            _saveStateDelayed();
        }
    }
}

#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION

String ClockPlugin::_getPowerLevelStr()
{
    PrintString powerLevelStr;
    auto level = _getPowerLevel();
    if (!std::isnormal(level)) {
        powerLevelStr = '0';
    }
    else {
        MQTT::Json::UnnamedTrimmedFormattedDouble(level, F("%.2f")).printTo(powerLevelStr);
    }
    if (_config.power_limit) {
        powerLevelStr.printf_P(PSTR(" / %u"), _config.power_limit);
    }
    return powerLevelStr;
}

void ClockPlugin::_updatePowerLevelWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(WebUINS::Values(F("pwrlvl"), _getPowerLevelStr()))));
    }
}

uint8_t ClockPlugin::_calcPowerFunction(uint8_t targetBrightness, uint32_t maxPower_mW)
{
    uint32_t requestedPower_mW;
    uint8_t newBrightness = _calculate_max_brightness_for_power_mW(targetBrightness, maxPower_mW, requestedPower_mW);
    if (targetBrightness && newBrightness == 0) { // brightness must not be 0
        newBrightness = 1;
    }
    // static int counter=0;
    // if (++counter%100==0)
    //     __DBG_printf("brightness=%u target=%u max=%umW requested=%umW", newBrightness, targetBrightness, maxPower_mW, requestedPower_mW);

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

void ClockPlugin::_createWebUI(WebUINS::Root &webUI)
{
    __LDBG_printf("createWebUI");
    #if IOT_LED_MATRIX
        webUI.addRow(WebUINS::Group(F("LED Matrix"), false));
    #else
        webUI.addRow(WebUINS::Group(F("Clock"), false));
    #endif


    webUI.addRow(WebUINS::Slider(FSPGM(brightness), FSPGM(brightness), 0, kMaxBrightness, true));
    webUI.addRow(WebUINS::RGBSlider(F("color"), F("Color")));

    // animation
    auto height = F("15rem");
    WebUINS::Row row;
    {
        constexpr uint8_t colspan = IOT_LED_MATRIX_WEBUI_COLSPAN_ANIMATION;

        #if IOT_CLOCK_SAVE_STATE
            auto power = WebUINS::Switch(F("power"), F("Power<div class=\"p-1\"></div><span class=\"oi oi-power-standby\">"), true, WebUINS::NamePositionType::TOP, colspan);
            row.append(power.append(WebUINS::NamedString(J(height), height)));
        #endif

        #if IOT_LED_MATRIX == 0
            auto colon = WebUINS::ButtonGroup(F("colon"), F("Colon"), F("{\"0\":\"Solid\",\"1000\":\"Blink slowly\",\"500\":\"Blink fast\"}"), 0, colspan);
            row.append(colon.append(WebUINS::NamedString(J(height), height)));
        #endif

        auto animation = WebUINS::Listbox(F("ani"), F("Animation"), Plugins::ClockConfig::ClockConfig_t::getAnimationNames(), false, 5, colspan);
        animation.append(WebUINS::NamedString(J(height), height));
        row.append(animation);

        #if IOT_CLOCK_TEMPERATURE_PROTECTION
            webUI.addRow(row);
        #endif
    }

    // protection
    {
        constexpr uint8_t colspan = IOT_LED_MATRIX_WEBUI_COLSPAN_PROTECTION;

        #if IOT_CLOCK_TEMPERATURE_PROTECTION
            webUI.addRow(WebUINS::Group(F("Protection"), false));
            WebUINS::Row row;

            {
                auto tempProtection = WebUINS::Sensor(F("tempp"), F("Temperature Protection"), '%', WebUINS::SensorRenderType::ROW, false, colspan);
                row.append(tempProtection.append(WebUINS::NamedString(J(height), height)));
            }
        #endif

        #if IOT_LED_MATRIX_FAN_CONTROL
        {
            auto fanSpeed = WebUINS::Slider(F("fanspeed"), F("Fan Speed<div class=\"p-1\"></div><span class=\"oi oi-fire\">"), _config.min_fan_speed - 1, _config.max_fan_speed, true, colspan);
            row.append(fanSpeed.append(
                WebUINS::NamedUint32(J(name), static_cast<uint32_t>(WebUINS::NamePositionType::TOP)),
                WebUINS::NamedString(J(height), height)
            ));
        }
        #endif

        // calculated power and power limit
        auto power = WebUINS::Sensor(F("pwrlvl"), getInstance()._config.power_limit ?
            #if IOT_SENSOR_HAVE_INA219
                F("Calculated Power / Limit") : F("Calculated Power"), 'W'

            #else
                F("Power / Limit") : F("Power"), 'W'
            #endif
        );
        power.append(
            WebUINS::NamedString(J(height), height),
            WebUINS::NamedUint32(J(columns), colspan)
        );
        row.append(power);

        #if IOT_CLOCK_HAVE_MOTION_SENSOR
        {
            auto motion = WebUINS::Sensor(F("motion"), F("Motion Sensor"), F(""));
            motion.append(
                WebUINS::NamedString(J(height), height),
                WebUINS::NamedUint32(J(columns), colspan)
            );
            row.append(motion);
        }
        #endif

        webUI.addRow(row);
    }

}

void ClockPlugin::webUIHook(WebUINS::Root &webUI, SensorPlugin::SensorType type)
{
    // add control to the top of the UI
    if (type == SensorPlugin::SensorType::MIN) {
        ClockPlugin::getInstance()._createWebUI(webUI);
        return;
    }
// //     // // calculated power and power limit
// #ifdef IOT_SENSOR_HAVE_INA219
//     if (type == SensorPlugin::SensorType::INA219) {
// //         webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(F("pwrlvl"), getInstance()._config.power_limit ? F("Calculated Power / Limit") : F("Calculated Power"), 'W')));
// #else
//     if (type == SensorPlugin::SensorType::SYSTEM_METRICS) {
// //         webUI.appendToLastRow(addRow::Row(WebUINS::Sensor(F("pwrlvl"), getInstance()._config.power_limit ? F("Power / Limit") : F("Power"), 'W')));
// #endif
//         IF_IOT_CLOCK_HAVE_MOTION_SENSOR(
//             webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(F("motion"), F("Motion Sensor"), F(""))));
//         )
//     }
}

void ClockPlugin::_broadcastWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUINS::Events events;
        getValues(events);
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(events));
    }
}

void ClockPlugin::_webUIUpdateColor(int color)
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(
            WebUINS::Values(F("color"), Color(color == -1 ? _getColor() : color).toString(), _getEnabledState() && _config.hasColorSupport())
        )));
    }
}

// #if IOT_LED_MATRIX_FAN_CONTROL

// void ClockPlugin::_webUIUpdateFanSpeed()
// {
//     if (WebUISocket::hasAuthenticatedClients()) {
//         WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(
//             WebUINS::Values(F("fanspeed"), _fanSpeed, true)
//         )));
//     }
// }

// #endif
