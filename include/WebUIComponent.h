/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_WEBUI
#define DEBUG_WEBUI                             1
#endif

#include <Arduino_compat.h>
#include <KFCJson.h>
#include <vector>
#include <memory>
#if MQTT_SUPPORT
#include "../src/plugins/mqtt/mqtt_strings.h"
#endif
#include "../src/plugins/mqtt/mqtt_json.h"

#if DEBUG_WEBUI
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define J(str)                                  FSPGM(webui_json_##str)
#if DEBUG_COLLECT_STRING_ENABLE
#define WEBUI_PROGMEM_STRING_DEF(str) \
    int __webui_json_##str = __debug_json_string_skip(String(__STRINGIFY(str))); \
    PROGMEM_STRING_DEF(webui_json_##str, _STRINGIFY(str));
#define WEBUI_PROGMEM_STRING_DEFVAL(str, val) \
    int __webui_json_##str = __debug_json_string_skip(String(val)); \
    PROGMEM_STRING_DEF(webui_json_##str, val);
#else
#define WEBUI_PROGMEM_STRING_DEF(str)           PROGMEM_STRING_DEF(webui_json_##str, _STRINGIFY(str))
#define WEBUI_PROGMEM_STRING_DEFVAL(str, val)   PROGMEM_STRING_DEF(webui_json_##str, val)
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
WEBUI_PROGMEM_STRING_DECL(medium)
WEBUI_PROGMEM_STRING_DECL(max)
WEBUI_PROGMEM_STRING_DECL(min)
WEBUI_PROGMEM_STRING_DECL(range_min)
WEBUI_PROGMEM_STRING_DECL(range_max)
WEBUI_PROGMEM_STRING_DECL(multi)
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
WEBUI_PROGMEM_STRING_DECL(update_events)
WEBUI_PROGMEM_STRING_DECL(value)
WEBUI_PROGMEM_STRING_DECL(vcc)
WEBUI_PROGMEM_STRING_DECL(width)
WEBUI_PROGMEM_STRING_DECL(zero_off)
WEBUI_PROGMEM_STRING_DECL(name)
WEBUI_PROGMEM_STRING_DECL(heading_top)
WEBUI_PROGMEM_STRING_DECL(heading_bottom)

namespace WebUINS {

    class Root;

    using namespace MQTT::Json;

    enum class AlignmentType : uint8_t {
        LEFT = 0,
        MIDDLE,
        CENTER = MIDDLE,
        RIGHT,
        DEFAULT_ALIGNMENT = LEFT,
    };

    enum class NamePositionType : uint8_t {
        HIDE = 0,
        SHOW = 1,
        TOP = 2,
    };

    enum class SensorRenderType {
        BADGE,
        ROW,
        COLUMN,
    };

    class Component : public UnnamedObject {
    public:
        using UnnamedObject::append;

        Component() {}

        template<typename ..._Args>
        Component(_Args &&...args) : UnnamedObject(std::forward<_Args&&>(args)...) {
        }

        template<typename _Ta>
        void setId(_Ta id) {
            append(createNamedString(J(id), id));
        }

        template<typename _Ta>
        void setName(_Ta title) {
            append(createNamedString(J(title), title));
        }

        template<typename _Ta>
        void setUnit(_Ta unit) {
            append(createNamedString(J(unit), unit));
        }

        // bootstrap layout col-N
        void setColumns(int columns) {
            append(NamedUint32(J(columns), columns));
        }

        // bootstrap layout col-offset-N
        void setOffset(int offset) {
            append(NamedUint32(J(offset), offset));
        }

        template<typename _Ta>
        void setValue(_Ta value) {
            append(createNamedString(J(value), value));
        }

        void setState(bool state) {
            append(NamedBool(J(state), state));
        }

        void append(SensorRenderType render) {
            switch(render) {
            case SensorRenderType::BADGE:
                append(NamedString(J(render_type), J(badge)));
                append(NamedUint32(J(columns), 2));
                break;
            case SensorRenderType::ROW:
                append(NamedString(J(render_type), J(row)));
                append(NamedUint32(J(columns), 4));
                break;
            case SensorRenderType::COLUMN:
                append(NamedString(J(render_type), J(columns)));
                break;
            }
        }

        template<typename _Ta, typename std::enable_if<std::is_same<__FlashStringHelper, _Ta>::value, int>::type = 0>
        inline static NamedString createNamedString(const __FlashStringHelper *key, const _Ta *value) {
            return NamedString(key, value);
        }

        template<typename _Ta, typename std::enable_if<std::is_base_of<String, _Ta>::value, int>::type = 0>
        inline static NamedStoredString createNamedString(const __FlashStringHelper *key, const _Ta &value) {
            return NamedStoredString(key, value);
        }

        template<typename _Ta, typename std::enable_if<std::is_same<char, _Ta>::value, int>::type = 0>
        inline static NamedChar createNamedString(const __FlashStringHelper *key, _Ta value) {
            return NamedChar(key, value);
        }

        inline static uint8_t colspanToColumns(uint8_t colspan) {
            if (colspan >= 101 && colspan <= 112) {
                return colspan;
            }
            switch(colspan) {
                case 12:
                    return 101;
                case 6:
                case 4:
                case 3:
                case 2:
                case 1:
                    return colspan;
            }
            return 0;
        }

    };

    class Sensor : public Component {
    public:
        template<typename _Ta, typename _Tb, typename _Tc>
        Sensor(_Ta id, _Tb title, _Tc unit, SensorRenderType render = SensorRenderType::ROW, bool binary = false) :
            Component(
                NamedString(J(type), binary ? J(binary_sensor) : J(sensor)),
                Component::createNamedString(J(id), id),
                Component::createNamedString(J(title), title),
                Component::createNamedString(J(unit), unit)
            )
        {
            append(render);
        }
    };

    class BadgeSensor : public Sensor {
    public:
        template<typename _Ta, typename _Tb, typename _Tc>
        BadgeSensor(_Ta id, _Tb title, _Tc unit) :
            Sensor(id, title, unit, SensorRenderType::BADGE)
        {}
    };

    class BinarySensor : public Sensor {
    public:
        template<typename _Ta, typename _Tb, typename _Tc>
        BinarySensor(_Ta id, _Tb title, _Tc unit, SensorRenderType render = SensorRenderType::ROW) :
            Sensor(id, title, unit, render, true)
        {}
    };

    class Group : public Component {
    public:
        // colspan
        // number of columns to use
        // 12 = entire row
        // 6 = lg, md: 50%, sm: 100%
        // 4 = lg, md: 33.3%, sm: 50%
        // 3 = lg: 25%, md: 33.3%, sm: 50%
        // 2 = lg: 16.6%, md:25%, sm: 33.3%
        // 1 = lg: 8.33%, md, sm: 16.6%
        // 101 - 112: colspan 1-12 for all screens
        template<typename _Ta>
        Group(_Ta title, bool hasSwitch, uint8_t colspan = 0) :
            Component(
                NamedString(J(type), J(group)),
                Component::createNamedString(J(title), title),
                NamedBool(J(has_switch), hasSwitch)
            )
        {
            colspan = colspanToColumns(colspan);
            if (colspan) {
                append(NamedUint32(J(columns), colspan));
            }
        }
    };


    class Switch : public Component {
    public:
        template<typename _Ta, typename _Tb>
        Switch(_Ta id, _Tb title, bool zeroOff = true, NamePositionType position = NamePositionType::HIDE, uint8_t colspan = 0) :
            Component(
                NamedString(J(type), J(switch)),
                Component::createNamedString(J(id), id),
                Component::createNamedString(J(title), title),
                NamedBool(J(zero_off), zeroOff),
                NamedUint32(J(name), static_cast<uint32_t>(position))
            )
        {
            colspan = colspanToColumns(colspan);
            if (colspan) {
                append(NamedUint32(J(columns), colspan));
            }
        }
    };

    class Slider : public Component {
    public:
        template<typename _Ta, typename _Tb>
        Slider(_Ta id, _Tb title, bool zeroOff = true, uint8_t colspan = 0) :
            Component(
                NamedString(J(type), J(slider)),
                Component::createNamedString(J(id), id),
                Component::createNamedString(J(title), title),
                NamedBool(J(zero_off), zeroOff)
            )
        {
            colspan = colspanToColumns(colspan);
            if (colspan) {
                append(NamedUint32(J(columns), colspan));
            }
        }
    };

    class ColorTemperatureSlider : public Component {
    public:
        template<typename _Ta, typename _Tb>
        ColorTemperatureSlider(_Ta id, _Tb title, uint8_t colspan = 0) :
            Component(
                NamedString(J(type), J(slider)),
                Component::createNamedString(J(id), id),
                Component::createNamedString(J(title), title),
                NamedString(J(color), J(temp))
            )
        {
            colspan = colspanToColumns(colspan);
            if (colspan) {
                append(NamedUint32(J(columns), colspan));
            }
        }
    };

    class RGBSlider : public Component {
    public:
        template<typename _Ta, typename _Tb>
        RGBSlider(_Ta id, _Tb title, uint8_t colspan = 0) :
            Component(
                NamedString(J(type), J(slider)),
                Component::createNamedString(J(id), id),
                Component::createNamedString(J(title), title),
                NamedString(J(color), J(rgb))
            )
        {
            colspan = colspanToColumns(colspan);
            if (colspan) {
                append(NamedUint32(J(columns), colspan));
            }
        }
    };

    class Screen : public Component {
    public:
        template<typename _Ta>
        Screen(_Ta id, uint16_t width, uint16_t height, uint8_t colspan = 0) :
            Component(
                NamedString(J(type), J(screen)),
                Component::createNamedString(J(id), id),
                NamedUint32(J(width), width),
                NamedUint32(J(height), height)
            )
        {
            colspan = colspanToColumns(colspan);
            if (colspan) {
                append(NamedUint32(J(columns), colspan));
            }
        }
    };

    class ButtonGroup : public Component {
    public:
        template<typename _Ta, typename _Tb, typename _Tc>
        ButtonGroup(_Ta id, _Tb title, _Tc buttons, uint8_t colspan = 0) :
            Component(
                NamedString(J(type), J(button_group)),
                Component::createNamedString(J(id), id),
                Component::createNamedString(J(title), title),
                Component::createNamedString(J(items), buttons)
            )
        {
            colspan = colspanToColumns(colspan);
            if (colspan) {
                append(NamedUint32(J(columns), colspan));
            }
        }
    };

    class Listbox : public Component {
    public:
        template<typename _Ta, typename _Tb, typename _Tc>
        Listbox(_Ta id, _Tb title, _Tc items, bool selectMultiple = false, uint8_t colspan = 0) :
            Component(
                NamedString(J(type), J(listbox)),
                Component::createNamedString(J(id), id),
                Component::createNamedString(J(title), title),
                Component::createNamedString(J(items), items)
            )
        {
            if (selectMultiple)   {
                append(NamedBool(J(multi), true));
            }
            colspan = colspanToColumns(colspan);
            if (colspan) {
                append(NamedUint32(J(columns), colspan));
            }
        }
    };


    class Row {
    public:
        friend Root;

        Row() {
        }

        template<typename ..._Args>
        Row(_Args &&...args) : _columns(std::forward<_Args&&>(args)...) {
        }

        template<typename _Ta, typename _Tb>
        void appendToLastColumn(_Ta id, _Tb value) {
            _columns.appendObject(2, NamedString(id, value));
        }

        void append(const Component &component) {
            _columns.append(component);
        }

        void clear() {
            _columns.clear();
        }

    protected:
        UnnamedArray _columns;
    };

    class Root {
    public:
        using UnnamedObject = MQTT::Json::UnnamedObject;
        using NamedArray = MQTT::Json::NamedArray;
        using NamedString = MQTT::Json::NamedString;
        using RemoveOuterArray = MQTT::Json::RemoveOuterArray;

        Root() {
            _json.append(NamedString(J(type), F("ui")));
            _json.append(NamedArray(F("data")));
        }

        void addRow(Row &row) {
            // append to _json.data
            _json.appendArray(2, UnnamedObject(
                NamedString(J(type), J(row)),
                NamedArray(F("columns"), RemoveOuterArray(row._columns))
            ));
            row.clear();
        }

        void addRow(const Row &row) {
            // append to _json.data
            _json.appendArray(2, UnnamedObject(
                NamedString(J(type), J(row)),
                NamedArray(F("columns"), RemoveOuterArray(row._columns))
            ));
        }

        void appendToLastRow(Row &row) {
            // append to _json.data[].columns
            if (_json.appendArray(4, RemoveOuterArray(row._columns)) != ErrorType::NONE) {
                // if no row exists, add to _json.data
                addRow(row);
            }
            else {
                row.clear();
            }
        }

        void appendToLastRow(const Row &row) {
            // append to _json.data[].columns
            if (_json.appendArray(4, RemoveOuterArray(row._columns)) != ErrorType::NONE) {
                // if no row exists, add to _json.data
                addRow(row);
            }
        }

        // retrieve values and states from all components
        void addValues();

        inline String toString() {
            return _json.toString();
        }

    private:
        UnnamedObject _json;
    };

    // helper class for formatting double and float
    // precision: is the number of digits (0-7), if an invalid value is passed, 2 will be used
    // format: custom printf format for double: for example "%05.3f"
    // trailing zeros are trimmed up to a single zero (1.00000 -> 1.0, 1.23000 -> 1.23)
    // if the value is not normal (inf, -inf, nan, ..) the json value will be set to null
    class TrimmedDouble {
    public:
        TrimmedDouble(double value, const __FlashStringHelper *format) : _value(value), _format(format) {}
        TrimmedDouble(double value, int precision = 2) : _value(value), _format(getPrecisionFormat(precision)) {}

        inline double getValue() const {
            return _value;
        }

        inline const __FlashStringHelper *getFormat() const {
            return _format;
        }

        inline bool isNormal() const {
            return std::isnormal(_value);
        }

        inline static const __FlashStringHelper *getPrecisionFormat(int precision) {
#if 0
            auto res = _getPrecisionFormat(precision);
            __DBG_printf("format=%s precision=%u", res, precision);
            return res;
        }

        inline static const __FlashStringHelper *_getPrecisionFormat(int precision) {
#endif
            switch (precision) {
            case 0:
                return F("%.0f");
            case 1:
                return F("%.1f");
            // case 2:
            //     return F("%.2f");
            case 3:
                return F("%.3f");
            case 4:
                return F("%.4f");
            case 5:
                return F("%.5f");
            case 6:
                return F("%.6f");
            case 7:
                return F("%.7f");
            default:
                break;
            }
            return F("%.2f");
        }

    private:
        double _value;
        const __FlashStringHelper *_format;
    };

    // helper class for double and float with zero digits
    // if the value is not normal (inf, nan, ..) the json value will be set to null
    class RoundedDouble : public TrimmedDouble {
    public:
        RoundedDouble(double value) : TrimmedDouble(value, 0) {}
    };


    class Values: public UnnamedObject {
    public:
        // do not add state
        static constexpr int8_t kStateNone = -2;
        // set state depending on the value
        // String, const char *, const __FlashStringHelper: empty string / strlen() == 0 is invalid. whitespaces are not trimmed
        // uint32_t: 0 and 4294967295 are invalid
        // int32_t: -2147483646, 0 and 2147483647 are invalid
        // uint8_t: 0 is invalid
        // double/float: !isnormal() is invalid (NAN, INF, -INF...)
        static constexpr int8_t kStateAuto = -1;
        static constexpr int8_t kStateTrue = true;
        static constexpr int8_t kStateFalse = false;

    public:
        template<typename _Ta>
        Values(const __FlashStringHelper *id, _Ta value, int8_t state = kStateAuto) :
            UnnamedObject(NamedString(J(id), id), getValueObject(value))
        {
            if (state == kStateAuto) {
                append(NamedBool(J(state), _validValue));
            } else if (state != kStateNone) {
                append(NamedBool(J(state), state));
            }
        }

        template<typename _Ta>
        Values(const String &id, _Ta value, int8_t state = kStateAuto) :
            UnnamedObject(NamedStoredString(J(id), id), getValueObject(value))
        {
            if (state == kStateAuto) {
                append(NamedBool(J(state), _validValue));
            }
            else if (state != kStateNone) {
                append(NamedBool(J(state), state));
            }
        }

    private:
        NamedString getValueObject(const __FlashStringHelper *value) {
            _validValue = pgm_read_byte(reinterpret_cast<PGM_P>(value)) != 0;
            return NamedString(J(value), value);
        }

        NamedStoredString getValueObject(const String &value) {
            _validValue = (value.length() != 0);
            return NamedStoredString(J(value), value);
        }

        NamedStoredString getValueObject(const char *value) {
            _validValue = *value != 0;
            return NamedStoredString(J(value), value);
        }

        NamedInt32 getValueObject(int32_t value) {
            _validValue = (value != 0) && (value != std::numeric_limits<int32_t>::min()) && (value != std::numeric_limits<int32_t>::max());
            return NamedInt32(J(value), value);
        }

        NamedUint32 getValueObject(uint32_t value) {
            _validValue = (value != 0) && (value != std::numeric_limits<int32_t>::max());
            return NamedUint32(J(value), value);
        }

        NamedUint32 getValueObject(uint8_t value) {
            _validValue = value != 0;
            return NamedUint32(J(value), static_cast<uint32_t>(value));
        }

        NamedBool getValueObject(bool value) {
            _validValue = true;
            return NamedBool(J(value), value);
        }

        NamedTrimmedFormattedDouble getValueObject(const TrimmedDouble &value) {
            _validValue = value.isNormal();
            return NamedTrimmedFormattedDouble(J(value), value.getValue(), value.getFormat());
        }

        bool _validValue;

    };

}

#if DEBUG_WEBUI
#include <debug_helper_disable.h>
#endif
