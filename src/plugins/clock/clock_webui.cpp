/**
 * Author: sascha_lammers@gmx.de
 */

#include "../src/plugins/sensor/sensor.h"
#include "clock.h"
#include <Arduino_compat.h>
#include <WebUISocket.h>
#include <stl_ext/algorithm.h>

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

void ClockPlugin::getValues(WebUINS::Events &array)
{
    auto enabled = _getEnabledState();

    #if IOT_LED_MATRIX == 0
        array.append(WebUINS::Values(F("colon"), enabled && (_config.blink_colon_speed < kMinBlinkColonSpeed) ? 0 : (_config.blink_colon_speed < 750 ? 500 : 1000), enabled));
    #endif

    auto brightness = WebUINS::Values(FSPGM(brightness), static_cast<uint8_t>(enabled ? _targetBrightness : _savedBrightness), true /* changing brightness can be used to turn the LEDs on */);
    #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR || IOT_CLOCK_TEMPERATURE_PROTECTION
        auto realBrightness = _getRealBrightnessTarget();
        if (enabled && realBrightness != _targetBrightness) {
            brightness.append(WebUINS::NamedUint32(F("rmin"), realBrightness));
        }
        else {
            brightness.append(WebUINS::NamedBool(F("rmin"), false));
        }
    #endif

    array.append(
        WebUINS::Values(F("ani"), static_cast<int>(_config.animation), enabled),
        WebUINS::Values(F("color"), Color(_getColor()).toString(), enabled && _config.hasColorSupport()),
        brightness
    );
    if (_tempBrightness == -1) {
        array.append(WebUINS::Values(F("tempp"), _overheatedInfo));
    } else {
        if (_tempBrightness == 1.0) {
            array.append(WebUINS::Values(F("tempp"), F("Off")));
        } else {
            array.append(WebUINS::Values(F("tempp"), WebUINS::FormattedDouble(100 - _tempBrightness * 100.0, 1)));
        }
    }

    array.append(WebUINS::Values(F("power"), static_cast<uint8_t>(enabled), true));

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        array.append(WebUINS::Values(F("pwrlvl"), _getPowerLevelStr()));
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        array.append(WebUINS::Values(F("fanspeed"), _fanSpeed, true));
    #endif
}

// bool ClockPlugin::getValue(const String &id, String &value, bool &state)
// {
//     bool result = false;
//     state = false;
//     if (id == F("animation-1")) {
//         state = _config.getAnimation() == AnimationType::RAINBOW;
//         //TODO check if the data can be created from the Forms class
//         value = PrintString(F("{\"_prefix\":\"#rb_\","
//             "\"mode\":%u,\"bpm\":%u,\"hue\":%u,"
//             "\"mul\":%.*f,\"incr\":%.*f,\"min\":%.*f,\"max\":%.*f,"
//             "\"sp\":%u,\"cf\":\"#%06X\",\"mv\":\"#%06X\","
//             "\"cre\":%.*f,\"cgr\":%.*f,\"cbl\":%.*f}"
//         ),
//             _config.rainbow._get_int_mode(),
//             _config.rainbow.bpm,
//             _config.rainbow.hue,
//             countDecimalPlaces(_config.rainbow.multiplier.value),
//             _config.rainbow.multiplier.value,
//             countDecimalPlaces(_config.rainbow.multiplier.incr),
//             _config.rainbow.multiplier.incr,
//             countDecimalPlaces(_config.rainbow.multiplier.min),
//             _config.rainbow.multiplier.min,
//             countDecimalPlaces(_config.rainbow.multiplier.max),
//             _config.rainbow.multiplier.max,
//             _config.rainbow.speed,
//             _config.rainbow.color.factor.value,
//             _config.rainbow.color.min.value,
//             countDecimalPlaces(_config.rainbow.color.red_incr),
//             _config.rainbow.color.red_incr,
//             countDecimalPlaces(_config.rainbow.color.green_incr),
//             _config.rainbow.color.green_incr,
//             countDecimalPlaces(_config.rainbow.color.blue_incr),
//             _config.rainbow.color.blue_incr
//         );
//         result = true;
//     }
//     __LDBG_printf("id=%s value=%s state=%u result=%u", id.c_str(), value.c_str(), state, result);
//     return result;
// }

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
                _saveState();
            }
            else
        #endif
            if (id == F("power")) {
            _setState(val);
        }
        #if IOT_LED_MATRIX_FAN_CONTROL
                if (id == F("fanspeed")) {
                _setFanSpeed(val);
                _config.fan_speed = _fanSpeed;
                _saveState();
                // if (val != _fanSpeed) {
                //     _webUIUpdateFanSpeed();
                // }
            }
            else
        #endif
        if (id == F("ani")) {
            setAnimation(static_cast<AnimationType>(val));
            _saveState();
        }
        else if (id.startsWith(F("ani-"))) {
            // create AsyncWebServerRequest from web socket post data and submit form
            auto request = WebServer::AsyncWebServerRequestParser(value);
            request.setUrl(PrintString(F("/" LED_MATRIX_MENU_URI_PREFIX "%s.html"), id.c_str()));
            request._tempObject = (void *)strdup_P(WebUISocket::hasAuthenticatedClients() ? WebUISocket::getSender()->getClient()->remoteIP().toString().c_str() : PSTR("[WebSocket]"));

            WebServer::Plugin::getInstance().handleFormData(id, &request, *this);
        }
        else if (id == F("color")) {
            setColorAndRefresh(val);
            _saveState();
        }
        else if (id == FSPGM(brightness)) {
            setBrightness(std::clamp<uint8_t>(val, 0, kMaxBrightness));
            _saveState();
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
            #if FASTLED_VERSION == 3004000 && (IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION)
                auto limit = FastLED.getPowerLimitScale();
                if (limit != 1.0f) {
                    powerLevelStr.printf_P(PSTR("<span style=\"font-size:0.75rem\">(%.1f%%)</span>"), limit * 100.0f);
                }
            #endif
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

    extern uint8_t _calculate_max_brightness_for_power_mW(uint8_t target_brightness, uint32_t max_power_mW, uint32_t &requested_power_mW);

    uint8_t ClockPlugin::_calcPowerFunction(uint8_t targetBrightness, uint32_t maxPower_mW)
    {
        uint32_t requestedPower_mW;
        uint8_t newBrightness = _calculate_max_brightness_for_power_mW(targetBrightness, maxPower_mW, requestedPower_mW);
        if (_isEnabled) {
            _powerLevelCurrentmW = (targetBrightness <= newBrightness) ? requestedPower_mW : maxPower_mW;
            _calcPowerLevel();
        }
        else {
            #if IOT_LED_MATRIX_STANDBY_PIN
                _powerLevelCurrentmW = 0;
            #else
                _powerLevelCurrentmW = calculate_idle_power_mW();
            #endif
            _powerLevelAvg = _powerLevelCurrentmW;
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
            auto diff = _powerLevelUpdateRate / static_cast<float>(get_time_since(_powerLevelUpdateTimer, ms));
            _powerLevelAvg = ((_powerLevelAvg * diff) + _powerLevelCurrentmW) / (diff + 1.0);
            _powerLevelUpdateTimer = ms;
        }
    }

#endif

void ClockPlugin::_createWebUI(WebUINS::Root &webUI)
{
    __LDBG_printf("createWebUI");
    #if IOT_LED_STRIP
        webUI.addRow(WebUINS::Group(F("LED Strip"), false));
    #elif IOT_LED_MATRIX
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

        auto power = WebUINS::Switch(F("power"), F("Power<div class=\"p-1\"></div><span class=\"oi oi-power-standby\">"), true, WebUINS::NamePositionType::TOP, colspan);
        row.append(power.append(WebUINS::NamedString(J(height), height)));

        #if IOT_LED_MATRIX == 0
            auto colon = WebUINS::ButtonGroup(F("colon"), F("Colon"), F("{\"0\":\"Solid\",\"1000\":\"Blink slowly\",\"500\":\"Blink fast\"}"), 0, colspan);
            row.append(colon.append(WebUINS::NamedString(J(height), height)));
        #endif

        auto animation = WebUINS::Listbox(F("ani"), F("Animation"), _config.getAnimationNames(), false, 5, colspan);
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
                auto tempProtection = WebUINS::Sensor(F("tempp"), F("Temperature Protection"), '%', false, colspan);
                tempProtection.append(WebUINS::SensorRenderType::COLUMN);
                row.append(tempProtection.append(WebUINS::NamedString(J(height), height)));
            }
        #endif

        #if IOT_LED_MATRIX_FAN_CONTROL
            {
                auto fanSpeed = WebUINS::Slider(F("fanspeed"), F("Fan Speed<div class=\"p-1\"></div><span class=\"oi oi-fire\">"), _config.min_fan_speed - 1, _config.max_fan_speed, true, colspan);
                row.append(fanSpeed.append(
                    WebUINS::NamedUint32(J(name), static_cast<uint32_t>(WebUINS::NamePositionType::TOP)),
                    WebUINS::NamedString(J(height), height)));
            }
        #endif

        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION

            // calculated power and power limit
            auto power = WebUINS::Sensor(F("pwrlvl"), getInstance()._config.power_limit ? F("LED Power / Limit") : F("LED Power"), 'W');
            power.append(
                WebUINS::NamedString(J(height), height),
                WebUINS::NamedUint32(J(columns), colspan));
            row.append(power);

        #endif

        #if IOT_CLOCK_HAVE_MOTION_SENSOR
            {
                auto motion = WebUINS::Sensor(F("motion"), F("Motion Sensor"), F(""));
                motion.append(
                    WebUINS::NamedString(J(height), height),
                    WebUINS::NamedUint32(J(columns), colspan));
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
    }
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
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(WebUINS::Values(F("color"), Color(color == -1 ? _getColor() : color).toString(), _getEnabledState() && _config.hasColorSupport()))));
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
