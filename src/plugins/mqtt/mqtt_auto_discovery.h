/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include "mqtt_strings.h"

class MQTTClient;
class MQTTComponent;

class MQTTAutoDiscovery {
public:
    enum class ComponentType : uint8_t {
        SWITCH = 1,
        LIGHT,
        SENSOR,
        BINARY_SENSOR,
        DEVICE_AUTOMATION
    };

    enum class FormatType : uint8_t {
        JSON,
        YAML,
        TOPIC,
    };

    // returns true to continue, false to stop (format TOPIC)
    bool create(MQTTComponent *component, const String &componentName, FormatType format);
    bool create(ComponentType componentType, const String &componentName, FormatType format);

public:
    inline void addParameter(const __FlashStringHelper *name, const char *value); // PROGMEM safe
    inline void addParameter(const __FlashStringHelper *name, const __FlashStringHelper *value);
    inline void addParameter(const __FlashStringHelper *name, const String &value);
    inline void addParameter(const __FlashStringHelper *name, char value);
    inline void addParameter(const __FlashStringHelper *name, int value);
    inline void addParameter(const __FlashStringHelper *name, unsigned value);
    inline void addParameter(const __FlashStringHelper *name, int64_t value);
    inline void addParameter(const __FlashStringHelper *name, uint64_t value);
    inline void addParameter(const __FlashStringHelper *name, float value);
    inline void addParameter(const __FlashStringHelper *name, double value);

public:
    template<typename _T>
    void addStateTopic(_T value) {
        addParameter(FSPGM(mqtt_state_topic), value);
    }

    template<typename _T>
    void addStateTopicAndPayloadOnOff(_T value) {
        addParameter(FSPGM(mqtt_state_topic), value);
        addPayloadOnOff();
    }

    template<typename _T>
    void addCommandTopic(_T value) {
        addParameter(FSPGM(mqtt_command_topic), value);
    }

    template<typename _T>
    void addBrightnessStateTopic(_T value) {
        addParameter(FSPGM(mqtt_brightness_state_topic), value);
    }

    template<typename _T>
    void addBrightnessCommandTopic(_T value) {
        addParameter(FSPGM(mqtt_brightness_command_topic), value);
    }

    template<typename _T>
    void addBrightnessScale(_T value) {
        addParameter(FSPGM(mqtt_brightness_scale), value);
    }

    template<typename _T>
    void addColorTempStateTopic(_T value) {
        addParameter(FSPGM(mqtt_color_temp_state_topic), value);
    }

    template<typename _T>
    void addColorTempCommandTopic(_T value) {
        addParameter(FSPGM(mqtt_color_temp_command_topic), value);
    }

    template<typename _T>
    void addRGBStateTopic(_T value) {
        addParameter(FSPGM(mqtt_rgb_state_topic), value);
    }

    template<typename _T>
    void addRGBCommandTopic(_T value) {
        addParameter(FSPGM(mqtt_rgb_command_topic), value);
    }

    template<typename _T>
    void addEffectCommandTopic(_T value) {
        addParameter(FSPGM(mqtt_effect_command_topic), value);
    }

    template<typename _T>
    void addEffectStateTopic(_T value) {
        addParameter(FSPGM(mqtt_effect_state_topic), value);
    }

    // JSON array
    // ["Efect1","Effect2",...]
    void addEffectList(String json) {
        __addParameter(FSPGM(mqtt_effect_list), json.c_str(), true);
    }

    template<typename _T>
    void addTopic(_T value) {
        addParameter(FSPGM(mqtt_topic), value);
    }

    template<typename _T>
    void addType(_T value) {
        addParameter(FSPGM(mqtt_type), value);
    }

    template<typename _T>
    void addubType(_T value) {
        addParameter(FSPGM(mqtt_subtype), value);
    }

    template<typename _T>
    void addExpireAfter(_T value) {
        addParameter(FSPGM(mqtt_expire_after), value);
    }

    void addValueTemplate(const char *value) { // PROGMEM safe
        PrintString value_json(F("{{ value_json.%s }}"), value);
        addParameter(FSPGM(mqtt_value_template), value_json.c_str());
    }

    void addValueTemplate(const String &value) {
        addValueTemplate(value.c_str());
    }

    template<typename _T>
    void addPayloadOn(_T value) {
        addParameter(FSPGM(mqtt_payload_on), value);
    }

    template<typename _T>
    void addPayloadOff(_T value) {
        addParameter(FSPGM(mqtt_payload_off), value);
    }

    void addPayloadOnOff() {
        addPayloadOn('1');
        addPayloadOff('0');
    }

    template<typename _T>
    void addUnitOfMeasurement(_T value) {
        addParameter(FSPGM(mqtt_unit_of_measurement), value);
    }

    void addAutomationType() {
        addParameter(FSPGM(mqtt_automation_type), F("trigger"));
    }

    void addSubType(const String &buttonName) {
        addParameter(FSPGM(mqtt_subtype), buttonName);
    }

    void addPayloadAndType(const String &buttonName, const __FlashStringHelper *type) {
        addParameter(FSPGM(mqtt_type), String(F("button_")) + String(type));
        addParameter(FSPGM(mqtt_payload), buttonName + '_' + String(type));
    }

public:
    void finalize();
    PrintString &getPayload();
    String &getTopic();

    // roughly the free space needed in the MQTT clients tcp buffer
    size_t getMessageSize() const;

    static bool isEnabled();

private:
    void __addParameter(const __FlashStringHelper *name, const char *value, bool list = false); // PROGMEM safe

    inline void __addParameter(const __FlashStringHelper *name, const __FlashStringHelper *value);
    inline void __addParameter(const __FlashStringHelper *name, const String &value);
    inline void __addParameter(const __FlashStringHelper *name, char value);

    template<typename _T>
    void __addParameter(const __FlashStringHelper *name, _T value) {
        PrintString str;
        str.print(value);
        addParameter(name, str);
    }

    static String getWildcardTopic();

private:
    bool _create(ComponentType componentType, const String &name, FormatType format);
    const String _getUnqiueId(const String &name);

    FormatType _format;
    PrintString _discovery;
    String _topic;
#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
    String _baseTopic;
#endif
};


inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, const char *value)
{
    __addParameter(name, value);
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, int value)
{
    __addParameter(name, value);
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, unsigned value)
{
    __addParameter(name, value);
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, int64_t value)
{
    __addParameter(name, value);
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, uint64_t value)
{
    __addParameter(name, value);
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, float value)
{
    __addParameter(name, static_cast<double>(value));
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, double value)
{
    __addParameter(name, value);
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, char value)
{
    __addParameter(name, value);
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, const __FlashStringHelper *value)
{
    __addParameter(name, RFPSTR(value));
}

inline void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, const String &value)
{
    __addParameter(name, value);
}

inline void MQTTAutoDiscovery::__addParameter(const __FlashStringHelper *name, char value)
{
    char str[2] = { value, 0 };
    __addParameter(name, str);
}

inline void MQTTAutoDiscovery::__addParameter(const __FlashStringHelper *name, const String &value)
{
    __addParameter(name, value.c_str());
}
