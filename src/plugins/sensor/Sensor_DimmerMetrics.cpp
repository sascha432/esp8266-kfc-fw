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
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(num) {
        case 0:
            if (discovery->create(this, F("int_temp"), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addValueTemplate(F("int_temp"));
                discovery->addDeviceClass(F("temperature"), FSPGM(UTF8_degreeC));
                discovery->addName(F("MCU Temperature"));
                discovery->addObjectId(baseTopic + F("mcu_temperature"));
            }
            break;
        case 1:
            if (discovery->create(this, F("ntc_temp"), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addValueTemplate(F("ntc_temp"));
                discovery->addDeviceClass(F("temperature"), FSPGM(UTF8_degreeC));
                discovery->addName(F("NTC Temperature"));
                discovery->addObjectId(baseTopic + F("ntc_temperature"));
            }
            break;
        case 2:
            if (discovery->create(this, FSPGM(vcc), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addValueTemplate(F("vcc"));
                discovery->addDeviceClass(F("voltage"), 'V');
                discovery->addName(F("MCU VCC"));
                discovery->addObjectId(baseTopic + F("mcu_vcc"));
            }
            break;
        case 3:
            if (discovery->create(this, FSPGM(frequency), format)) {
                discovery->addStateTopic(_getMetricsTopics());
                discovery->addValueTemplate(FSPGM(frequency));
                discovery->addDeviceClass(F("frequency"), FSPGM(Hz, "Hz"));
                discovery->addName(F("Mains Frequency"));
                discovery->addObjectId(baseTopic + F("mains_frequency"));
            }
            break;
    }
    return discovery;
}

void Sensor_DimmerMetrics::getValues(WebUINS::Events &array, bool timer)
{
    array.append(
        WebUINS::Values(F("ntc_temp"), WebUINS::TrimmedFloat(_metrics.metrics.get_ntc_temp(), 1), _metrics.metrics.has_ntc_temp()),
        WebUINS::Values(F("int_temp"), WebUINS::TrimmedFloat(_metrics.metrics.get_int_temp(), 1), _metrics.metrics.has_int_temp()),
        WebUINS::Values(F("vcc"), WebUINS::TrimmedFloat(_metrics.metrics.get_vcc(), 3), _metrics.metrics.has_vcc()),
        WebUINS::Values(F("frequency"), WebUINS::TrimmedFloat(_metrics.metrics.get_frequency(), 2), _metrics.metrics.has_frequency())
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
        using namespace MQTT::Json;
        publish(_getMetricsTopics(), true, UnnamedObject(
            NamedTrimmedFormattedDouble(F("int_temp"), _metrics.metrics.get_int_temp(), UnnamedFormattedDouble::getPrecisionFormat(2)),
            NamedTrimmedFormattedDouble(F("ntc_temp"), _metrics.metrics.get_ntc_temp(), UnnamedFormattedDouble::getPrecisionFormat(2)),
            NamedTrimmedFormattedDouble(F("vcc"), _metrics.metrics.get_vcc(), UnnamedFormattedDouble::getPrecisionFormat(2)),
            NamedFormattedDouble(F("frequency"), _metrics.metrics.get_frequency(), UnnamedFormattedDouble::getPrecisionFormat(2))
        ).toString());
    }
}

#endif
