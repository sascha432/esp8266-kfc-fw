/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR

#include <EventScheduler.h>
#include <WebUISocket.h>
#include <ReadADC.h>
#include "Sensor_AmbientLight.h"
#include "sensor.h"

#undef DEBUG_IOT_SENSOR
#define DEBUG_IOT_SENSOR 1

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_AmbientLight::Sensor_AmbientLight(const String &name) :
    MQTT::Sensor(MQTT::SensorType::AMBIENT_LIGHT),
    _name(name),
    _handler(nullptr),
    _sensor({SensorType::NONE}),
    _config(Plugins::Sensor::getConfig().ambient),
    _value(-1)

{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_AmbientLight::~Sensor_AmbientLight()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_AmbientLight::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    switch(num) {
        case 0: {
            if (!discovery->create(MQTTComponent::ComponentType::SENSOR, F("light_sensor"), format)) {
                return discovery;
            }
            discovery->addStateTopic(MQTTClient::formatTopic(F("light_sensor")));
            switch(_sensor.type) {
                // case SensorType::BH1750FVI:
                //     discovery->addUnitOfMeasurement(F("lx"));
                //     break;
                default:
                    discovery->addUnitOfMeasurement(String('%'));
                    break;
            }
        }
        break;
    }
    return discovery;
}

void Sensor_AmbientLight::getValues(WebUINS::Events &array, bool timer)
{
    array.append(WebUINS::Values(FSPGM(light_sensor), _getLightSensorWebUIValue()));
}

void Sensor_AmbientLight::createWebUI(WebUINS::Root &webUI)
{
    WebUINS::Row row(WebUINS::Sensor(_getId(), _name, emptyString));
    webUI.appendToLastRow(row);
}

void Sensor_AmbientLight::publishState()
{
    // __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (isConnected()) {
        publish(MQTTClient::formatTopic(FSPGM(light_sensor)), true, String(getAutoBrightness(), 0));
    }
}

void Sensor_AmbientLight::getStatus(Print &output)
{
    output.print(F("IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR"));
    output.println(F(HTML_S(br)));
}

void Sensor_AmbientLight::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    auto &group = form.addCardGroup(F("alscfg"), F(IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR), true);

    form.addObjectGetterSetter(F("auto_br"), FormGetterSetter(cfg.ambient, auto_brightness));
    form.addFormUI(F("Auto Brightness Value"), FormUI::SuffixHtml(F("<span class=\"input-group-text\">0-1023</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>")));
    cfg.ambient.addRangeValidatorFor_auto_brightness(form);

    group.end();
}

void Sensor_AmbientLight::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig().ambient;
}

void Sensor_AmbientLight::begin(AmbientLightSensorHandler *handler, const SensorConfig &sensor)
{
    uint32_t interval;
    _handler = nullptr;
    _sensor = sensor;
    switch(_sensor.type) {
        case SensorType::BH1750FVI:
            // power up and configure high res mode, continuously, 1lux, 120ms
            Wire.beginTransmission(_sensor.bh1750FVI.i2cAddress);
            Wire.write(BH1750FVI_POWER_UP);
            if (Wire.endTransmission() == 0) {
                delay(10); // wait for power up
                Wire.beginTransmission(_sensor.bh1750FVI.i2cAddress);
                Wire.write(BH1750FVI_MODE_HIGHRES);
                Wire.endTransmission();
            }
            interval = 240;
            break;
        case SensorType::INTERNAL_ADC:
            ADCManager::getInstance().addAutoReadTimer(Event::seconds(1), Event::milliseconds(30), 24);
            interval = 750;
            break;
        default:
            interval = 250;
            break;

    }
    _timerCallback(interval);
    _handler = handler;
    _handler->_ambientLightSensor = this;
    _timer.add(Event::milliseconds(interval), true, [this](Event::CallbackTimerPtr timer) {
        _timerCallback(timer->getShortInterval());
    });
}

void Sensor_AmbientLight::end()
{
    _timer.remove();
    _handler->_ambientLightSensor = nullptr;
    _handler = nullptr;
    switch(_sensor.type) {
        case SensorType::BH1750FVI:
            Wire.beginTransmission(_sensor.bh1750FVI.i2cAddress);
            Wire.write(BH1750FVI_POWER_DOWN);
            Wire.endTransmission();
            break;
        default:
            break;
    }
}

void Sensor_AmbientLight::_timerCallback(uint32_t interval)
{
    switch(_sensor.type) {
        case SensorType::BH1750FVI:
            _value = _readBH1750FVI();
            break;
        case SensorType::TINYPWM:
            _value = _readTinyPwmADC();
            break;
        case SensorType::INTERNAL_ADC:
            _value = ADCManager::getInstance().readValue();
            break;
        default:
            break;
    }
    if (_handler && _handler->isAutobrightnessEnabled()) {
        float value = std::clamp(_value / static_cast<float>(_config.auto_brightness), 0.1f, 1.0f);
        if (_handler->_autoBrightnessValue == 0) {
            _handler->_autoBrightnessValue = value;
        }
        else {
            const float period = 2500 / interval;
            const float count = period + 1;
            _handler->_autoBrightnessValue = ((_handler->_autoBrightnessValue * period) + value) / count; // integrate over 2.5 seconds to get a smooth transition and avoid flickering
        }
#if DEBUG_IOT_SENSOR
        static uint32_t start;
        if ((start - millis()) > 1000U) {
            start = millis();
            __LDBG_printf("value=%d auto_brightness=%d brightness=%.3f", _value, _config.auto_brightness, _handler->_autoBrightnessValue);
        }
#endif
    }

}

const __FlashStringHelper *Sensor_AmbientLight::_getId()
{
    return F("light_sensor");
}

String Sensor_AmbientLight::_getTopic()
{
    return MQTTClient::formatTopic(_getId());
}

String Sensor_AmbientLight::_getLightSensorWebUIValue()
{
    if (!_handler || !_handler->isAutobrightnessEnabled()) {
        return PrintString(F("<strong>OFF</strong><br> <span class=\"light-sensor-value\">Sensor value %d</span>"), getValue());
    }
    return PrintString(F("%.0f &#37;"), getAutoBrightness());
}

void Sensor_AmbientLight::_updateLightSensorWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(
            WebUINS::Events(WebUINS::Values(_getId(), _getLightSensorWebUIValue()))
        ));
    }
}

int32_t Sensor_AmbientLight::_readTinyPwmADC()
{
    uint16_t level;
    Wire.beginTransmission(_sensor.tinyPWM.i2cAddress);
    Wire.write(_sensor.tinyPWM.adcPin);
    Wire.write(0x00);
    if (
        (Wire.endTransmission() == 0) &&
        (Wire.requestFrom(_sensor.tinyPWM.i2cAddress, sizeof(level)) == sizeof(level)) &&
        (Wire.readBytes(reinterpret_cast<uint8_t *>(&level), sizeof(level)) == sizeof(level))
     ) {
         if (_sensor.tinyPWM.inverted) {
            level = 1023 - level;
         }
        __LDBG_printf("TinyPWM light sensor level: %u", level);
        if (isnan(_sensor.tinyPWM.level)) {
            _sensor.tinyPWM.level = level;
        }
        else {
            auto mul = 2000 / static_cast<float>(get_time_diff(_sensor.tinyPWM.lastUpdate, millis()));
            _sensor.tinyPWM.level = ((_sensor.tinyPWM.level * mul) + level) / (mul + 1.0);
        }
        _sensor.tinyPWM.lastUpdate = millis();
        return level;
    }
    return -1;
}

int32_t Sensor_AmbientLight::_readBH1750FVI()
{
    uint16_t level;
    if (Wire.requestFrom(_sensor.bh1750FVI.i2cAddress, sizeof(level)) == sizeof(level)) {
        level = Wire.read() << 8;
        level |= Wire.read();
        __LDBG_printf("BH1750FVI light sensor level: %u", level);

/*

https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf

The below formula is to calculate illuminance per 1 count.
H-reslution mode : Illuminance per 1 count ( lx / count ) = 1 / 1.2 *( 69 / X )
H-reslution mode2 : Illuminance per 1 count ( lx / count ) = 1 / 1.2 *( 69 / X ) / 2
1.2 : Measurement accuracy
69 : Default value of MTreg ( dec )
X : MTreg value

*/
        // return value is not lux
        // TODO implement displaying lux
        return level;
    }
    return -1;
}


#endif
