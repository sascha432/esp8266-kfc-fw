/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX

#include <Arduino_compat.h>
#include "clock.h"
#include <WebUISocket.h>
#include "./plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void ClockPlugin::getValues(JsonArray &array)
{
    auto obj = &array.addObject(3);

    obj = &array.addObject(3);
    obj->add(JJ(id), F("btn_animation"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _config.animation); //static_cast<int>(_config.animation));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("color"));
    obj->add(JJ(state), static_cast<AnimationType>(_config.animation) == AnimationType::NONE);
    obj->add(JJ(value), _color.get());

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(brightness));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _targetBrightness);

    obj = &array.addObject(3);
    obj->add(JJ(id), F("temp_prot"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(100 - _tempBrightness * 100.0, 1));

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
        auto val = (uint8_t)value.toInt();
        if (String_equals(id, PSTR("btn_animation"))) {
            setAnimation(static_cast<AnimationType>(val));
        }
        else if (String_equals(id, PSTR("color"))) {
            setColorAndRefresh(value.toInt());
        }
        else if (String_equals(id, SPGM(brightness))) {
            setBrightness(value.toInt());
        }
    }
}

void ClockPlugin::createWebUI(WebUIRoot &webUI)
{
    auto row = &webUI.addRow();
    row->addGroup(F("LED Matrix"), false);

    row = &webUI.addRow();
    row->addSlider(FSPGM(brightness), FSPGM(brightness), 0, SevenSegmentDisplay::kMaxBrightness, true);

    row = &webUI.addRow();
    row->addRGBSlider(F("color"), F("Color"));

    row = &webUI.addRow();
    auto height = F("15rem");
    row->addButtonGroup(F("btn_animation"), F("Animation"), F("Solid,Rainbow,Flashing,Fading")).add(JJ(height), height);
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    row->addSensor(FSPGM(light_sensor), F("Ambient Light Sensor"), F("<img src=\"/images/light.svg\" width=\"80\" height=\"80\" style=\"margin-top:-20px;margin-bottom:1rem\">"), WebUIComponent::SensorRenderType::COLUMN).add(JJ(height), height);
#endif

    row->addSensor(F("temp_prot"), F("Temperature Protection"), '%');
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

#endif
