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
    } ComponentTypeEnum_t;

    typedef enum {
        FORMAT_JSON = 0,
        FORMAT_YAML = 1,
    } Format_t;

    void create(MQTTComponent *component, uint8_t count, Format_t format);
    void create(MQTTComponent *component, const String &componentName, Format_t format);
    void addParameter(const String &name, const String &value);

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

    inline void addParameter(const String &name, int value) {
        addParameter(name, String(value));
    }
    inline void addParameter(const String &name, char value) {
        addParameter(name, String(value));
    }
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

    static bool isEnabled();

private:
    void _create(MQTTComponent *component, const String &name, Format_t format);
    const String _getUnqiueId(const String &name);

    Format_t _format;
    PrintString _discovery;
    String _topic;
};
