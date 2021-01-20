/**
 * Author: sascha_lammers@gmx.de
 */

#include "WebUIComponent.h"
#include "plugins.h"

#if DEBUG_WEBUI
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

WEBUI_PROGMEM_STRING_DEF(align)
WEBUI_PROGMEM_STRING_DEF(badge)
WEBUI_PROGMEM_STRING_DEF(binary_sensor)
WEBUI_PROGMEM_STRING_DEF(button_group)
WEBUI_PROGMEM_STRING_DEF(center)
WEBUI_PROGMEM_STRING_DEF(color)
WEBUI_PROGMEM_STRING_DEF(columns)
WEBUI_PROGMEM_STRING_DEF(events)
WEBUI_PROGMEM_STRING_DEF(data)
WEBUI_PROGMEM_STRING_DEF(group)
WEBUI_PROGMEM_STRING_DEF(has_switch)
WEBUI_PROGMEM_STRING_DEF(height)
WEBUI_PROGMEM_STRING_DEF(id)
WEBUI_PROGMEM_STRING_DEF(items)
WEBUI_PROGMEM_STRING_DEF(left)
WEBUI_PROGMEM_STRING_DEF(listbox)
WEBUI_PROGMEM_STRING_DEF(max)
WEBUI_PROGMEM_STRING_DEF(medium)
WEBUI_PROGMEM_STRING_DEF(min)
WEBUI_PROGMEM_STRING_DEF(offset)
WEBUI_PROGMEM_STRING_DEF(render_type)
WEBUI_PROGMEM_STRING_DEF(right)
WEBUI_PROGMEM_STRING_DEF(rgb)
WEBUI_PROGMEM_STRING_DEF(row)
WEBUI_PROGMEM_STRING_DEF(screen)
WEBUI_PROGMEM_STRING_DEF(sensor)
WEBUI_PROGMEM_STRING_DEF(slider)
WEBUI_PROGMEM_STRING_DEF(state)
WEBUI_PROGMEM_STRING_DEF(switch)
WEBUI_PROGMEM_STRING_DEF(temp)
WEBUI_PROGMEM_STRING_DEF(title)
WEBUI_PROGMEM_STRING_DEF(top)
WEBUI_PROGMEM_STRING_DEF(type)
WEBUI_PROGMEM_STRING_DEF(unit)
WEBUI_PROGMEM_STRING_DEF(ue)
WEBUI_PROGMEM_STRING_DEF(ui)
WEBUI_PROGMEM_STRING_DEF(value)
WEBUI_PROGMEM_STRING_DEF(vcc)
WEBUI_PROGMEM_STRING_DEF(width)
WEBUI_PROGMEM_STRING_DEF(zero_off)
WEBUI_PROGMEM_STRING_DEF(name)
WEBUI_PROGMEM_STRING_DEF(ht)
WEBUI_PROGMEM_STRING_DEF(hb)


WebUIRow::WebUIRow() : JsonUnnamedObject(2)
{
    add(JJ(type), JJ(row));
}

void WebUIRow::setName(const JsonString &name)
{
    add(JJ(title), name);
}

void WebUIRow::setAlignment(AlignmentEnum_t alignment)
{
    switch(alignment) {
        case CENTER:
            add(JJ(align), JJ(center));
            break;
        case RIGHT:
            add(JJ(align), JJ(right));
            break;
        case LEFT:
            add(JJ(align), JJ(left));
            break;
    }
}

WebUIComponent &WebUIRow::addColumn(size_t reserve)
{
    auto &column = *__LDBG_new(WebUIComponent, reserve);
    _getColumns().add(reinterpret_cast<AbstractJsonValue *>(&column));
    return column;
}

JsonArray &WebUIRow::_getColumns()
{
    auto columns = find(J(columns));
    if (!columns) {
        return addArray(JJ(columns));
    }
    return reinterpret_cast<JsonArray &>(*columns);
}

WebUIComponent &WebUIRow::addGroup(const JsonString &name, bool hasSwitch)
{
    WebUIComponent &column = addColumn(3);
    column.add(JJ(type), JJ(group));
    column.setName(name);
    column.add(JJ(has_switch), hasSwitch);
    return column;
}

WebUIComponent &WebUIRow::addSwitch(const String &id, const JsonString &name, bool zeroOff, NamePositionType position)
{
    WebUIComponent &column = addColumn(4);
    column.add(JJ(type), JJ(switch));
    column.setId(id);
    column.setName(name);
    column.add(JJ(zero_off), zeroOff);
    column.add(JJ(name), static_cast<uint8_t>(position));
    return column;
}

WebUIComponent &WebUIRow::addSlider(const String &id, const JsonString &name, int min, int max, bool zeroOff)
{
    WebUIComponent &column = addColumn(6);
    column.add(JJ(type), JJ(slider));
    column.setId(id);
    column.setName(name);
    column.add(JJ(min), min);
    column.add(JJ(max), max);
    column.add(JJ(zero_off), zeroOff);
    return column;
}

WebUIComponent &WebUIRow::addColorTemperatureSlider(const String &id, const JsonString &name, const JsonString &color)
{
    WebUIComponent &column = addColumn(4);
    column.add(JJ(type), JJ(slider));
    column.setId(id);
    column.setName(name);
    column.add(JJ(color), color.length() ? color : JJ(temp));
    return column;
}

WebUIComponent &WebUIRow::addRGBSlider(const String &id, const JsonString &name)
{
    return addColorTemperatureSlider(id, name, JJ(rgb));
}

WebUIComponent &WebUIRow::addSensor(const String &id, const JsonString &name, const JsonString &unit, WebUIComponent::SensorRenderType render)
{
    WebUIComponent &column = addColumn(6);
    column.add(JJ(type), JJ(sensor));
    column.setId(id);
    column.setName(name);
    column.setUnit(unit);
    switch(render) {
        case WebUIComponent::SensorRenderType::BADGE:
            column.add(JJ(render_type), JJ(badge));
            column.setColumns(2);
            break;
        case WebUIComponent::SensorRenderType::ROW:
            column.add(JJ(render_type), JJ(row));
            column.setColumns(4);
            break;
        case WebUIComponent::SensorRenderType::COLUMN:
            column.add(JJ(render_type), JJ(columns));
            break;
    }
    return column;
}

WebUIComponent &WebUIRow::addBadgeSensor(const String &id, const JsonString &name, const JsonString &unit)
{
    return addSensor(id, name, unit, WebUIComponent::SensorRenderType::BADGE);
}

WebUIComponent &WebUIRow::addBinarySensor(const String &id, const JsonString &name, const JsonString &unit, WebUIComponent::SensorRenderType render)
{
    WebUIComponent &column = addSensor(id, name, unit, render);
    column.replace(JJ(type), JJ(binary_sensor));
    return column;
}

WebUIComponent &WebUIRow::addScreen(const String &id, uint16_t width, uint16_t height)
{
    WebUIComponent &column = addColumn(6);
    column.add(JJ(type), JJ(screen));
    column.setId(id);
    column.add(JJ(width), width);
    column.add(JJ(height), height);
    return column;
}

WebUIComponent &WebUIRow::addButtonGroup(const String &id, const JsonString &name, const JsonString &buttons)
{
    WebUIComponent &column = addColumn(6);
    column.add(JJ(type), JJ(button_group));
    column.setId(id);
    column.setName(name);
    column.add(JJ(items), buttons);
    return column;
}

WebUIComponent &WebUIRow::addListbox(const String &id, const JsonString &name, const JsonString &items, bool selectMultiple)
{
    WebUIComponent &column = addColumn(6);
    column.add(JJ(type), JJ(listbox));
    column.setId(id);
    column.setName(name);
    column.add(JJ(items), items);
    if (selectMultiple) {
        column.add(F("multi"), true);
    }
    return column;
}

WebUI::WebUI(JsonUnnamedObject &json) : _json(json)
{
    _json.add(JJ(type), JJ(ui));
    _rows = &_json.addArray(JJ(data));
}

WebUIRow &WebUI::addRow()
{
    auto rowPtr = __LDBG_new(WebUIRow);
    _rows->add(reinterpret_cast<AbstractJsonValue *>(rowPtr));
    return *rowPtr;
}

void WebUI::addValues()
{
    auto &array = _json.addArray(F("values"));
    for(auto plugin: plugins) {
        if (plugin->hasWebUI()) {
            plugin->getValues(array);
        }
    }
}
