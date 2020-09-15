/**
 * Author: sascha_lammers@gmx.de
 */

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
    obj->add(JJ(id), F("btn_colon"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), (_config.blink_colon_speed < kMinBlinkColonSpeed) ? 0 : (_config.blink_colon_speed < 750 ? 2 : 1));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("btn_animation"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), static_cast<int>(_config.animation));

    int color = 3;
    switch(_color.get()) {
        case 0xff0000:
            color = 0;
            break;
        case 0x00ff00:
            color = 1;
            break;
        case 0x0000ff:
            color = 2;
            break;
    }

    obj = &array.addObject(3);
    obj->add(JJ(id), F("btn_color"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), color);

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(brightness));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _targetBrightness);

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
        else if (String_equals(id, PSTR("btn_animation"))) {
            setAnimation(static_cast<AnimationType>(val));
        }
        else if (String_equals(id, PSTR("btn_color"))) {
            Color color;
            switch(val) {
                case 0:
                    color = Color(255, 0, 0);
                    break;
                case 1:
                    color = Color(0, 255, 0);
                    break;
                case 2:
                    color = Color(0, 0, 255);
                    break;
                case 3:
                default:
                    color.rnd();
                    break;
            }
            setColorAndRefresh(color);
        }
        else if (String_equals(id, SPGM(brightness))) {
            setBrightness(value.toInt());
        }
    }
}

void ClockPlugin::createWebUI(WebUIRoot &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Clock"), false);

    row = &webUI.addRow();
    row->addSlider(FSPGM(brightness), FSPGM(brightness), 0, SevenSegmentDisplay::kMaxBrightness, true);

    row = &webUI.addRow();
    static const uint16_t height = 280;
    row->addButtonGroup(F("btn_colon"), F("Colon"), F("Solid,Blink slowly,Blink fast"), height);
    row->addButtonGroup(F("btn_animation"), F("Animation"), F("Solid,Rainbow,Flashing,Fading"), height);
    row->addButtonGroup(F("btn_color"), F("Color"), F("Red,Green,Blue,Random"), height);
    auto &sensor = row->addSensor(FSPGM(light_sensor), F("Ambient Light Sensor"), F("<br><img src=\"/images/light.svg\" width=\"80\" height=\"80\" style=\"margin-top:10px\">"));
    sensor.add(JJ(height), height);
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
