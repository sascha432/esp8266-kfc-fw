/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if MQTT_SUPPORT

#include <Arduino_compat.h>
#include <PrintString.h>

class MQTTClient;
class MQTTComponent;

class MQTTAutoDiscovery {
public:
    typedef enum : uint8_t {
        FORMAT_JSON = 0,
        FORMAT_YAML = 1,
    } Format_t;

    void create(MQTTComponent *component, Format_t format = FORMAT_JSON);
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
    void addUnitOfMeasurement(const String &value);
    void addValueTemplate(const String &value);

    void finalize();
    String getPayload();
    String getTopic();

    static bool isEnabled();

private:
    const String _getUnqiueId(const String &name);

    Format_t _format;
    PrintString _discovery;
    String _topic;
};

#endif
