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
    typedef enum {
        FORMAT_JSON = 0,
        FORMAT_YAML = 1,
    } Format_t;

    void create(MQTTComponent *component, Format_t format = FORMAT_JSON);
    void addParameter(const String &name, const String &value);
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
