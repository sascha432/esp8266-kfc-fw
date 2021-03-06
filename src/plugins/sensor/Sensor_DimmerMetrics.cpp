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

MQTT::AutoDiscovery::EntityPtr Sensor_DimmerMetrics::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new AutoDiscovery::Entity();
    switch(num) {
        case 0:
            if (discovery->create(this, F("int_temp"), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addValueTemplate(F("int_temp"));
                discovery->addDeviceClass(F("temperature"), FSPGM(UTF8_degreeC));
            }
            break;
        case 1:
            if (discovery->create(this, F("ntc_temp"), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addValueTemplate(F("ntc_temp"));
                discovery->addDeviceClass(F("temperature"), FSPGM(UTF8_degreeC));
            }
            break;
        case 2:
            if (discovery->create(this, FSPGM(vcc), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addValueTemplate(F("vcc"));
                discovery->addDeviceClass(F("voltage"), 'V');
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

void Sensor_DimmerMetrics::getValues(WebUINS::Events &array, bool timer)
{
    array.append(
        WebUINS::Values(F("ntc_temp"), WebUINS::TrimmedDouble(_metrics.metrics.get_ntc_temp(), 1), _metrics.metrics.has_ntc_temp()),
        WebUINS::Values(F("int_temp"), WebUINS::TrimmedDouble(_metrics.metrics.get_int_temp(), 1), _metrics.metrics.has_int_temp()),
        WebUINS::Values(F("vcc"), WebUINS::TrimmedDouble(_metrics.metrics.get_vcc(), 3), _metrics.metrics.has_vcc()),
        WebUINS::Values(F("frequency"), WebUINS::FormattedDouble(_metrics.metrics.get_freqency(), 2), _metrics.metrics.has_frequency())
    );
}

void Sensor_DimmerMetrics::_createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Row(
        WebUINS::BadgeSensor(FSPGM(vcc), _name, 'V'),
        WebUINS::BadgeSensor(FSPGM(frequency), F("Frequency"), FSPGM(Hz)),
        WebUINS::BadgeSensor(F("int_temp"), F("ATmega"), FSPGM(UTF8_degreeC)),
        WebUINS::BadgeSensor(F("ntc_temp"), F("NTC"), FSPGM(UTF8_degreeC))
    ));
    _webUIinitialized = true;
}

void Sensor_DimmerMetrics::publishState()
{
    if (isConnected()) {
        publish(_getMetricsTopics(), true, WebUINS::UnnamedObject(
            WebUINS::NamedTrimmedFormattedDouble(F("int_temp"), _metrics.metrics.get_int_temp(), WebUINS::FormattedDouble::getPrecisionFormat(2)),
            WebUINS::NamedTrimmedFormattedDouble(F("ntc_temp"), _metrics.metrics.get_ntc_temp(), WebUINS::FormattedDouble::getPrecisionFormat(2)),
            WebUINS::NamedTrimmedFormattedDouble(F("vcc"), _metrics.metrics.get_vcc(), WebUINS::FormattedDouble::getPrecisionFormat(3)),
            WebUINS::NamedFormattedDouble(F("frequency"), _metrics.metrics.get_freqency(), WebUINS::FormattedDouble::getPrecisionFormat(2))
        ).toString());
    }
}

#endif
