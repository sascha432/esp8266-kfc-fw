/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>

class MQTTClient;
class MQTTComponent;

class MQTTAutoDiscovery {
public:
    typedef enum {
        SWITCH = 1,
        LIGHT,
        SENSOR,
        BINARY_SENSOR,
        STORAGE,
    } ComponentTypeEnum_t;

    using ComponentType = ComponentTypeEnum_t;

    enum class FormatType : uint8_t {
        JSON,
        YAML,
    };

    void create(MQTTComponent *component, const String &componentName, FormatType format);
    void create(ComponentTypeEnum_t componentType, const String &componentName, FormatType format);

public:
    void addParameter(const __FlashStringHelper *name, const String &value);
    inline void addParameter(const __FlashStringHelper *name, int value) {
        addParameter(name, String(value));
    }
    inline void addParameter(const __FlashStringHelper *name, char value) {
        addParameter(name, String(value));
    }

public:
    void addStateTopic(const String &value);
    void addCommandTopic(const String &value);
    void addPayloadOn(const String &value);
    void addPayloadOff(const String &value);
    void addBrightnessStateTopic(const String &value);
    void addBrightnessCommandTopic(const String &value);
    void addBrightnessScale(uint32_t brightness);
    void addColorTempStateTopic(const String &value);
    void addColorTempCommandTopic(const String &value);
    void addRGBStateTopic(const String &value);
    void addRGBCommandTopic(const String &value);
    void addUnitOfMeasurement(const String &value);
    void addValueTemplate(const String &value);

    inline void addPayloadOn(int value) {
        addPayloadOn(String(value));
    }
    inline void addPayloadOn(char value) {
        addPayloadOn(String(value));
    }
    inline void addPayloadOff(int value) {
        addPayloadOff(String(value));
    }
    inline void addPayloadOff(char value) {
        addPayloadOff(String(value));
    }
    inline void addUnitOfMeasurement(char value) {
        addUnitOfMeasurement(String(value));
    }

    void finalize();
    PrintString &getPayload();
    String &getTopic();

    // roughly the free space needed in the MQTT clients tcp buffer
    size_t getMessageSize() const;

    static bool isEnabled();

private:
    void _create(ComponentTypeEnum_t componentType, const String &name, FormatType format);
    const String _getUnqiueId(const String &name);

    FormatType _format;
    PrintString _discovery;
    String _topic;
#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
    String _baseTopic;
#endif
};
