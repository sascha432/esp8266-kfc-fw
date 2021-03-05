/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ATOMIC_SUN_V2 || IOT_DIMMER_MODULE

#include "Sensor_DimmerMetrics.h"
#include "../src/plugins/dimmer_module/firmware_protocol.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>

Sensor_DimmerMetrics::Sensor_DimmerMetrics(const String &name) : MQTT::Sensor(SensorType::DIMMER_METRICS), _name(name), _webUIinitialized(false)
{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_DimmerMetrics::~Sensor_DimmerMetrics()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_DimmerMetrics::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = __LDBG_new(AutoDiscovery::Entity);
    switch(num) {
        case 0:
            if (discovery->create(this, F("int_temp"), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addUnitOfMeasurement(FSPGM(degree_Celsius_unicode));
                discovery->addValueTemplate(F("int_temp"));
                discovery->addDeviceClass(F("temperature"));
            }
            break;
        case 1:
            if (discovery->create(this, F("ntc_temp"), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addUnitOfMeasurement(FSPGM(degree_Celsius_unicode));
                discovery->addValueTemplate(F("ntc_temp"));
                discovery->addDeviceClass(F("temperature"));
            }
            break;
        case 2:
            if (discovery->create(this, FSPGM(vcc), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addUnitOfMeasurement('V');
                discovery->addValueTemplate(F("vcc"));
                discovery->addDeviceClass(F("voltage"));
            }
            break;
        case 3:
            if (discovery->create(this, FSPGM(frequency), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addValueTemplate(FSPGM(frequency));
                discovery->addUnitOfMeasurement(FSPGM(Hz, "Hz"));
            }
            break;
    }
    return discovery;
}

uint8_t Sensor_DimmerMetrics::getAutoDiscoveryCount() const
{
    return 4;
}

void Sensor_DimmerMetrics::getValues(NamedJsonArray &array, bool timer)
{
    using namespace MQTT::Json;
    array.append(
        UnnamedObject(
            NamedString(F("id"), F("ntc_temp")),
            NamedBool(F("state"), _metrics.metrics.has_ntc_temp()),
            NamedVariant<FStr, TrimmedDouble>(F("value"), TrimmedDouble(_metrics.metrics.get_ntc_temp(), 1))
        ),
        UnnamedObject(
            NamedString(F("id"), F("int_temp")),
            NamedBool(F("state"), _metrics.metrics.has_int_temp()),
            NamedVariant<FStr, TrimmedDouble>(F("value"), TrimmedDouble(_metrics.metrics.get_int_temp(), 1))
        ),
        UnnamedObject(
            NamedString(F("id"), F("vcc")),
            NamedBool(F("state"), _metrics.metrics.has_vcc()),
            NamedVariant<FStr, TrimmedDouble>(F("value"), TrimmedDouble(_metrics.metrics.get_vcc(), 3))
        ),
        UnnamedObject(
            NamedString(F("id"), F("frequency")),
            NamedBool(F("state"), _metrics.metrics.has_frequency()),
            NamedDouble(F("value"), FormattedDouble(_metrics.metrics.get_freqency(), 2))
        )
    );
}

void Sensor_DimmerMetrics::getValues(JsonArray &array, bool timer)
{
    JsonUnnamedObject *obj;

    obj = &array.addObject(3);
    obj->add(JJ(id), F("ntc_temp"));
    obj->add(JJ(state), _metrics.metrics.has_ntc_temp());
    obj->add(JJ(value), JsonNumber(_metrics.metrics.get_ntc_temp(), 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("int_temp"));
    obj->add(JJ(state), _metrics.metrics.has_int_temp());
    obj->add(JJ(value), JsonNumber(_metrics.metrics.get_int_temp(), 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(vcc));
    obj->add(JJ(state), _metrics.metrics.has_vcc());
    obj->add(JJ(value), JsonNumber(_metrics.metrics.get_vcc(), 3));

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(frequency));
    obj->add(JJ(state), !_metrics.metrics.has_frequency());
    obj->add(JJ(value), JsonNumber(_metrics.metrics.get_freqency(), 2));
}

void Sensor_DimmerMetrics::_createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    (*row)->addBadgeSensor(FSPGM(vcc), _name, 'V');
    (*row)->addBadgeSensor(FSPGM(frequency), F("Frequency"), FSPGM(Hz));
    (*row)->addBadgeSensor(F("int_temp"), F("ATmega"), FSPGM(degree_Celsius_html));
    (*row)->addBadgeSensor(F("ntc_temp"), F("NTC"), FSPGM(degree_Celsius_html));
    _webUIinitialized = true;
}

void Sensor_DimmerMetrics::createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    if (!_webUIinitialized) {
        _createWebUI(webUI, row);
    }
}

void Sensor_DimmerMetrics::publishState()
{
    if (isConnected()) {
        using namespace MQTT::Json;
        publish(_getMetricsTopics(), true, UnnamedObject(
            NamedDouble(F("int_temp"), FormattedDouble(_metrics.metrics.get_int_temp(), 2)),
            NamedDouble(F("ntc_temp"), FormattedDouble(_metrics.metrics.get_ntc_temp(), 2)),
            NamedDouble(F("vcc"), FormattedDouble(_metrics.metrics.get_vcc(), 3)),
            NamedDouble(F("frequency"), FormattedDouble(_metrics.metrics.get_freqency(), 2))
        ).toString());
    }
}

void Sensor_DimmerMetrics::getStatus(Print &output)
{
}

Sensor_DimmerMetrics::MetricsType &Sensor_DimmerMetrics::_updateMetrics(const MetricsType &metrics)
{
    _metrics = metrics;
    return _metrics;
}

#endif
