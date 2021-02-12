/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_WEBUI
#define DEBUG_WEBUI                             0
#endif

#include <Arduino_compat.h>
#include <KFCJson.h>
#include <vector>
#include <memory>

#define J(str)                                  FSPGM(webui_json_##str)
#define JJ(str)                                 JsonString(FSPGM(webui_json_##str), true)
#if DEBUG_COLLECT_STRING_ENABLE
#define WEBUI_PROGMEM_STRING_DEF(str) \
    int __webui_json_##str = __debug_json_string_skip(String(__STRINGIFY(str))); \
    PROGMEM_STRING_DEF(webui_json_##str, _STRINGIFY(str));
#else
#define WEBUI_PROGMEM_STRING_DEF(str)           PROGMEM_STRING_DEF(webui_json_##str, _STRINGIFY(str))
#endif
#define WEBUI_PROGMEM_STRING_DECL(str)          PROGMEM_STRING_DECL(webui_json_##str)


WEBUI_PROGMEM_STRING_DECL(align)
WEBUI_PROGMEM_STRING_DECL(badge)
WEBUI_PROGMEM_STRING_DECL(binary_sensor)
WEBUI_PROGMEM_STRING_DECL(button_group)
WEBUI_PROGMEM_STRING_DECL(center)
WEBUI_PROGMEM_STRING_DECL(color)
WEBUI_PROGMEM_STRING_DECL(columns)
WEBUI_PROGMEM_STRING_DECL(events)
WEBUI_PROGMEM_STRING_DECL(data)
WEBUI_PROGMEM_STRING_DECL(group)
WEBUI_PROGMEM_STRING_DECL(has_switch)
WEBUI_PROGMEM_STRING_DECL(head)
WEBUI_PROGMEM_STRING_DECL(height)
WEBUI_PROGMEM_STRING_DECL(id)
WEBUI_PROGMEM_STRING_DECL(items)
WEBUI_PROGMEM_STRING_DECL(left)
WEBUI_PROGMEM_STRING_DECL(listbox)
WEBUI_PROGMEM_STRING_DECL(max)
WEBUI_PROGMEM_STRING_DECL(medium)
WEBUI_PROGMEM_STRING_DECL(min)
WEBUI_PROGMEM_STRING_DECL(offset)
WEBUI_PROGMEM_STRING_DECL(render_type)
WEBUI_PROGMEM_STRING_DECL(right)
WEBUI_PROGMEM_STRING_DECL(rgb)
WEBUI_PROGMEM_STRING_DECL(row)
WEBUI_PROGMEM_STRING_DECL(screen)
WEBUI_PROGMEM_STRING_DECL(sensor)
WEBUI_PROGMEM_STRING_DECL(slider)
WEBUI_PROGMEM_STRING_DECL(state)
WEBUI_PROGMEM_STRING_DECL(switch)
WEBUI_PROGMEM_STRING_DECL(temp)
WEBUI_PROGMEM_STRING_DECL(title)
WEBUI_PROGMEM_STRING_DECL(top)
WEBUI_PROGMEM_STRING_DECL(type)
WEBUI_PROGMEM_STRING_DECL(unit)
WEBUI_PROGMEM_STRING_DECL(ue)
WEBUI_PROGMEM_STRING_DECL(ui)
WEBUI_PROGMEM_STRING_DECL(value)
WEBUI_PROGMEM_STRING_DECL(vcc)
WEBUI_PROGMEM_STRING_DECL(width)
WEBUI_PROGMEM_STRING_DECL(zero_off)
WEBUI_PROGMEM_STRING_DECL(name)
WEBUI_PROGMEM_STRING_DECL(ht)
WEBUI_PROGMEM_STRING_DECL(hb)

class WebUIComponent : public JsonUnnamedObject {
public:
    enum class SensorRenderType {
        BADGE,
        ROW,
        COLUMN,
    };

    using JsonUnnamedObject::JsonUnnamedObject;

    void setId(const String &id) {
        add(JJ(id), id);
    }

    void setName(const JsonString &name) {
        add(JJ(title), name);
    }

    void setUnit(const JsonString &unit) {
        add(JJ(unit), unit);
    }

    // bootstrap layout col-N
    void setColumns(int columns) {
        add(JJ(columns), columns);
    }

    // bootstrap layout col-offset-N
    void setOffset(int offset) {
        add(JJ(offset), offset);
    }

    void setValue(const JsonString &value) {
        add(JJ(value), value);
    }

    void setState(bool state) {
        add(JJ(state), state);
    }
};

class WebUIRow : public JsonUnnamedObject {
public:
    typedef enum {
        LEFT = 0,
        MIDDLE,
        CENTER = MIDDLE,
        RIGHT,
        DEFAULT_ALIGNMENT = LEFT,
    } AlignmentEnum_t;

    enum class NamePositionType : uint8_t {
        HIDE = 0,
        SHOW = 1,
        TOP = 2,
    };

    WebUIRow();
    void setName(const JsonString &name);
    void setAlignment(AlignmentEnum_t alignment);
    WebUIComponent &addColumn(size_t reserve = 0);
    JsonArray &_getColumns();
    WebUIComponent &addGroup(const JsonString &name, bool hasSwitch);
    WebUIComponent &addSwitch(const String &id, const JsonString &name, bool zeroOff = true, NamePositionType position = NamePositionType::HIDE);
    WebUIComponent &addSlider(const String &id, const JsonString &name, int min = 0, int max = 0, bool zeroOff = true);
    WebUIComponent &addColorTemperatureSlider(const String &id, const JsonString &name, const JsonString &color = JsonString());
    WebUIComponent &addRGBSlider(const String &id, const JsonString &name);
    WebUIComponent &addSensor(const String &id, const JsonString &name, const JsonString &unit, WebUIComponent::SensorRenderType render = WebUIComponent::SensorRenderType::ROW);
    WebUIComponent &addBadgeSensor(const String &id, const JsonString &name, const JsonString &unit);
    WebUIComponent &addBinarySensor(const String &id, const JsonString &name, const JsonString &unit, WebUIComponent::SensorRenderType render = WebUIComponent::SensorRenderType::ROW);
    WebUIComponent &addScreen(const String &id, uint16_t width, uint16_t height);
    WebUIComponent &addButtonGroup(const String &id, const JsonString &name, const JsonString &buttons);
    WebUIComponent &addListbox(const String &id, const JsonString &name, const JsonString &items, bool selectMultiple = false);

};

class WebUI {
public:
    WebUI(JsonUnnamedObject &json);
    WebUIRow &addRow();

    // retrieve values and states fromm all components
    void addValues();

    inline JsonUnnamedObject &getJsonObject() {
        return _json;
    }

private:
    JsonUnnamedObject &_json;
    JsonArray *_rows;
};

using WebUIRoot = ::WebUI;
