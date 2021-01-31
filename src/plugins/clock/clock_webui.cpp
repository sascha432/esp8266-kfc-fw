/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <WebUISocket.h>
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
    // obj->add(JJ(state), true);
    obj->add(JJ(value), _config.animation); //static_cast<int>(_config.animation));

#if !IOT_LED_MATRIX
    obj = &array.addObject(2);
    obj->add(JJ(id), F("btn_colon"));
    // obj->add(JJ(state), true);
    obj->add(JJ(value), (_config.blink_colon_speed < kMinBlinkColonSpeed) ? 0 : (_config.blink_colon_speed < 750 ? 2 : 1));
#endif

    obj = &array.addObject(2);
    obj->add(JJ(id), F("color"));
#if IOT_LED_MATRIX
    obj->add(JJ(state), _config.getAnimation() != AnimationType::RAINBOW && _config.getAnimation() != AnimationType::FIRE);
#else
    obj->add(JJ(state), _config.getAnimation() != AnimationType::RAINBOW);
#endif
    obj->add(JJ(value), _color.get());

    obj = &array.addObject(2);
    obj->add(JJ(id), FSPGM(brightness));
    // obj->add(JJ(state), true);
    obj->add(JJ(value), _targetBrightness);

    obj = &array.addObject(2);
    obj->add(JJ(id), F("temp_prot"));
    // obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(100 - _tempBrightness * 100.0, 1));

#if IOT_CLOCK_SAVE_STATE
    obj = &array.addObject(2);
    obj->add(JJ(id), F("power"));
    obj->add(JJ(value), _targetBrightness);
#endif

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(light_sensor));
    obj->add(JJ(value), _getLightSensorWebUIValue());
    obj->add(JJ(state), true);
#endif
}

void ClockPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u value=%s has_state=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        _resetAlarm();
        auto val = (uint32_t)value.toInt();
#if !IOT_LED_MATRIX
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
        }
        else
#endif
#if IOT_CLOCK_SAVE_STATE
        if (String_equals(id, PSTR("power"))) {
            _setState(val);
        }
        else
#endif
        if (String_equals(id, PSTR("btn_animation"))) {
            setAnimation(static_cast<AnimationType>(val));
            _saveStateDelayed();
        }
        else if (String_equals(id, PSTR("color"))) {
            setColorAndRefresh(val);
            _saveStateDelayed();
        }
        else if (String_equals(id, SPGM(brightness))) {
            setBrightness(val);
            _saveStateDelayed();
        }
    }
}

void ClockPlugin::createWebUI(WebUIRoot &webUI)
{
    auto row = &webUI.addRow();
#if IOT_LED_MATRIX
    row->addGroup(F("LED Matrix"), false);
#else
    row->addGroup(F("Clock"), false);
#endif

    row = &webUI.addRow();
    row->addSlider(FSPGM(brightness), FSPGM(brightness), 0, SevenSegmentDisplay::kMaxBrightness, true);

    row = &webUI.addRow();
    row->addRGBSlider(F("color"), F("Color"));

    row = &webUI.addRow();
    auto height = F("15rem");

#if IOT_CLOCK_SAVE_STATE
    row->addSwitch(F("power"), F("Power<div class=\"p-1\"></div><span class=\"oi oi-power-standby\">"), true, WebUIRow::NamePositionType::TOP).add(JJ(height), height);
#endif

#if !IOT_LED_MATRIX
    row->addButtonGroup(F("btn_colon"), F("Colon"), F("Solid,Blink slowly,Blink fast")).add(JJ(height), height);
#endif

    auto &col = row->addButtonGroup(F("btn_animation"), F("Animation"), Plugins::ClockConfig::ClockConfig_t::getAnimationNames());
    col.add(JJ(height), height);
#if IOT_LED_MATRIX
    col.add(JJ(row), 3);
#endif

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    row->addSensor(FSPGM(light_sensor), F("Ambient Light Sensor"), F("<img src=\"/images/light.svg\" width=\"80\" height=\"80\" style=\"margin-top:-20px;margin-bottom:1rem\">"), WebUIComponent::SensorRenderType::COLUMN).add(JJ(height), height);
#endif

    row->addSensor(F("temp_prot"), F("Temperature Protection"), '%').add(JJ(height), height);
}

void ClockPlugin::_broadcastWebUI()
{
    if (WsWebUISocket::getWsWebUI() && WsWebUISocket::hasClients(WsWebUISocket::getWsWebUI())) {
        JsonUnnamedObject json(2);
        json.add(JJ(type), JJ(ue));
        getValues(json.addArray(JJ(events)));
        WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
    }
}
