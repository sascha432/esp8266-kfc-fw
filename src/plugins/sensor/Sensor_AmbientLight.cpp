/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR

#include <EventScheduler.h>
#include <WebUISocket.h>
#include <ReadADC.h>
#include "Sensor_AmbientLight.h"
#include "sensor.h"

// #undef DEBUG_IOT_SENSOR
// #define DEBUG_IOT_SENSOR 1

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_AmbientLight::Sensor_AmbientLight(const String &name, uint8_t id) :
    MQTT::Sensor(MQTT::SensorType::AMBIENT_LIGHT),
    _name(name),
    _handler(nullptr),
    _sensor({SensorType::NONE}),
    _config(Plugins::Sensor::getConfig().ambient),
    _value(-1),
    _id(id)

{
    REGISTER_SENSOR_CLIENT(this);
    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2_BH1750FVI_I2C_ADDRESS
        // auto setup for lux sensor
        if (_id == 1) {
            auto config = SensorConfig(SensorType::BH1750FVI);
            config.bh1750FVI = SensorConfig::BH1750FVI(IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2_BH1750FVI_I2C_ADDRESS, true);
            begin(nullptr, config);
        }
    #endif
}

Sensor_AmbientLight::~Sensor_AmbientLight()
{
    end();
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_AmbientLight::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    switch(num) {
        case 0: {
            if (!discovery->create(MQTTComponent::ComponentType::SENSOR, _getId(), format)) {
                return discovery;
            }
            discovery->addStateTopic(_getTopic());
            if (_id == 1 && _sensor.type == Sensor_AmbientLight::SensorType::BH1750FVI && _sensor.bh1750FVI.highRes) {
                discovery->addUnitOfMeasurement(F("lux"));
            }
            else {
                discovery->addUnitOfMeasurement(String('%'));
            }
        }
        break;
    }
    return discovery;
}

void Sensor_AmbientLight::getValues(WebUINS::Events &array, bool timer)
{
    array.append(WebUINS::Values(_getId(), _getLightSensorWebUIValue()));
}

void Sensor_AmbientLight::createWebUI(WebUINS::Root &webUI)
{
    auto title = (_id == 1) ? F(IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR2) : F(IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR);
    auto sensor = WebUINS::Sensor(_getId(), title, F("<img src=\"/images/light.svg\" width=\"80\" height=\"80\" style=\"margin-top:-20px;margin-bottom:1rem\">"), IOT_SENSOR_AMBIENT_LIGHT_RENDER_TYPE);
    #ifdef IOT_SENSOR_AMBIENT_LIGHT_RENDER_HEIGHT
        sensor.append(WebUINS::NamedString(J(height), IOT_SENSOR_AMBIENT_LIGHT_RENDER_HEIGHT));
    #endif
    WebUINS::Row row(sensor);
    webUI.appendToLastRow(row);
}

void Sensor_AmbientLight::publishState()
{
    // __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (isConnected()) {
        auto value = (_sensor.type == SensorType::BH1750FVI && _sensor.bh1750FVI.highRes) ? String(_sensor.bh1750FVI.illuminance, 1) : String(getAutoBrightness(), 0);
        publish(_getTopic(), true, value);
    }
}

void Sensor_AmbientLight::getStatus(Print &output)
{
    if (_id == 1) {
        output.print(F(IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR2 HTML_S(br)));
    }
    else {
        output.print(F(IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR HTML_S(br)));
    }
}

void Sensor_AmbientLight::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    if (_id == 0) {
        auto &cfg = Plugins::Sensor::getWriteableConfig();
        auto &group = form.addCardGroup(F("alscfg"), F(IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR), true);

        int32_t maxBrightness;
        auto suffix = F("<span class=\"input-group-text\">0-1023</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>");
        #if 0
        // high res mode currently not supported
        if (_sensor.type == Sensor_AmbientLight::SensorType::BH1750FVI && _sensor.bh1750FVI.highRes) {
            maxBrightness = 65535;
            suffix = F("<span class=\"input-group-text\">0-65535 lux</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>");
        }
        else
        #endif
        {
            switch(_sensor.type) {
                case Sensor_AmbientLight::SensorType::INTERNAL_ADC:
                    maxBrightness = ADCManager::kMaxADCValue;
                    break;
                case Sensor_AmbientLight::SensorType::BH1750FVI:
                case Sensor_AmbientLight::SensorType::TINYPWM:
                default:
                    maxBrightness = 1023;
                    break;
            }
        }

        form.addObjectGetterSetter(F("auto_br"), FormGetterSetter(cfg.ambient, auto_brightness));
        form.addFormUI(F("Auto Brightness Value"),
            FormUI::SuffixHtml(suffix),
            FormUI::PlaceHolder(cfg.ambient.kDefaultValueFor_auto_brightness),
            FormUI::MinMax(cfg.ambient.kMinValueFor_auto_brightness, maxBrightness)
        );
        form.addValidator(FormUI::Validator::Range(static_cast<long>(cfg.ambient.kMinValueFor_auto_brightness), static_cast<long>(maxBrightness)));

        form.addObjectGetterSetter(F("auto_bras"), FormGetterSetter(cfg.ambient, adjustment_speed));
        form.addFormUI(F("Adjustment Speed"));
        cfg.ambient.addRangeValidatorFor_adjustment_speed(form);

        group.end();
    }
}

void Sensor_AmbientLight::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig().ambient;
}

void Sensor_AmbientLight::begin(AmbientLightSensorHandler *handler, const SensorConfig &sensor)
{
    uint32_t interval = 125;
    _handler = nullptr;
    _sensor = sensor;
    switch(_sensor.type) {
        case SensorType::BH1750FVI: {
                config.initTwoWire();
                // power up
                Wire.beginTransmission(_sensor.bh1750FVI.i2cAddress);
                Wire.write(BH1750FVI_POWER_UP);
                Wire.endTransmission();
                delay(10); // wait for power up
                Wire.beginTransmission(_sensor.bh1750FVI.i2cAddress);
                if (_sensor.bh1750FVI.highRes) {
                    // configure high res mode, continuously, 0.5lux, min. interval 120ms
                    interval = 1000;
                    Wire.write(BH1750FVI_MODE_HIGHRES2);
                }
                else {
                    // configure low res mode, continuously, 4lux, min. interval 16ms
                    interval = 125;
                    Wire.write(BH1750FVI_MODE_LOWRES);
                }
                #if DEBUG_IOT_SENSOR
                    uint8_t status =
                #endif
                    Wire.endTransmission();
                #if DEBUG_IOT_SENSOR
                    if (status != 0) {
                        __LDBG_printf("BH1750FVI status=%u addr=%02x highres=%u", status, _sensor.bh1750FVI.i2cAddress, _sensor.bh1750FVI.highRes);
                        return;
                    }
                #endif
            } break;
        case SensorType::INTERNAL_ADC:
            ADCManager::getInstance().addAutoReadTimer(Event::seconds(1), Event::milliseconds(30), 24);
            break;
        default:
            break;

    }
    _timerCallback();
    _handler = handler;
    if (_handler) {
        _handler->_ambientLightSensor = this;
    }
    _timer.add(Event::milliseconds(interval), true, [this](Event::CallbackTimerPtr timer) {
        _timerCallback();
    });
}

void Sensor_AmbientLight::end()
{
    _timer.remove();
    if (_handler) {
        _handler->_ambientLightSensor = nullptr;
        _handler = nullptr;
    }
    switch(_sensor.type) {
        case SensorType::BH1750FVI:
            Wire.beginTransmission(_sensor.bh1750FVI.i2cAddress);
            Wire.write(BH1750FVI_POWER_DOWN);
            Wire.endTransmission();
            break;
        default:
            break;
    }
    _sensor.type = SensorType::NONE;
}

void Sensor_AmbientLight::_timerCallback()
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
            if (_sensor.adc.inverted) {
                _value = ADCManager::kMaxADCValue - _value;
            }
            break;
        default:
            break;
    }
    if (_handler && _handler->isAutobrightnessEnabled()) {
        const uint32_t speed = ((_config.adjustment_speed << 4) + 1000);
        if (_config.auto_brightness == -1) {
            _handler->_autoBrightnessValue = 1;
        }
        else {
            float value = std::clamp(_value / static_cast<float>(_config.auto_brightness), 0.1f, 1.0f);
            if (_handler->_autoBrightnessValue == 0) {
                _handler->_autoBrightnessValue = value;
            }
            else {
                const float period = speed / 125;
                const float count = period + 1;
                _handler->_autoBrightnessValue = ((_handler->_autoBrightnessValue * period) + value) / count; // integrate over 2.5 seconds to get a smooth transition and avoid flickering
            }
        }
#if DEBUG_IOT_SENSOR
        static uint32_t start;
        if ((millis() - start) > 10000U) {
            start = millis();
            __LDBG_printf("value=%d auto_br=%d br=%.3f adj=%u", _value, _config.auto_brightness, _handler->_autoBrightnessValue, speed);
        }
#endif
    }

}

String Sensor_AmbientLight::_getId()
{
    return PrintString(F("light_sensor_%02x"), _id);
}

String Sensor_AmbientLight::_getTopic()
{
    return MQTTClient::formatTopic(_getId());
}

String Sensor_AmbientLight::_getLightSensorWebUIValue()
{
    if (_sensor.type == SensorType::BH1750FVI && _sensor.bh1750FVI.highRes) {
        return PrintString(F("%.1f lux"), _sensor.bh1750FVI.illuminance);
    }
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
        if (!_sensor.bh1750FVI.highRes) {
            level >>= 6;
            _sensor.bh1750FVI.illuminance = NAN;
        }
        else {
            // https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf

            // The below formula is to calculate illuminance per 1 count.
            // H-reslution mode : Illuminance per 1 count ( lx / count ) = 1 / 1.2 *( 69 / X )
            // H-reslution mode2 : Illuminance per 1 count ( lx / count ) = 1 / 1.2 *( 69 / X ) / 2
            // 1.2 : Measurement accuracy
            // 69 : Default value of MTreg ( dec )
            // X : MTreg value
            // _sensor.bh1750FVI.illuminance = (level * 115) / (BH1750FVI_MTREG_DEFAULT * 2.0);  // mode
            // _sensor.bh1750FVI.illuminance = level * (115 / (BH1750FVI_MTREG_DEFAULT * 2.0));  // mode
            // _sensor.bh1750FVI.illuminance = (level * 115) / (BH1750FVI_MTREG_DEFAULT * 4.0);  // mode2
            _sensor.bh1750FVI.illuminance = level * (115 / (BH1750FVI_MTREG_DEFAULT * 4.0));  // mode2
        }

#if DEBUG_IOT_SENSOR
        static uint32_t start;
        if ((millis() - start) > 10000U) {
            start = millis();
            __LDBG_printf("BH1750FVI light sensor level=%u illuminance=%.1f", level, _sensor.bh1750FVI.illuminance);
        }
#endif
        return level;
    }
    else {
        __LDBG_printf("BH1750FVI requestFrom failed");
    }
    return -1;
}


#endif
