/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_base.h"
#include <KFCJson.h>
#include "WebUISocket.h"
#include "../mqtt/mqtt_client.h"
#include "dimmer_module.h"
#if IOT_ATOMIC_SUN_V2
#include "../atomic_sun/atomic_sun_v2.h"
#endif
#include "serial_handler.h"

#include "../../trailing_edge_dimmer/src/dimmer_protocol.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART
Dimmer_Base::Dimmer_Base() : _serial(Serial), _wire(*new SerialTwoWire(serial)) {
#else
Dimmer_Base::Dimmer_Base() : _wire(Wire) {
#endif
}

Dimmer_Base::~Dimmer_Base() {
}

void Dimmer_Base::_begin() {
#if IOT_DIMMER_MODULE_INTERFACE_UART
    _wire->onReadSerial(SerialHandler::serialLoop);
    _wire->begin(DIMMER_I2C_ADDRESS + 1);
    _wire->onReceive(Dimmer_Base::onReceive);
#else
    _wire.begin(IOT_DIMMER_MODULE_INTERFACE_SDA, IOT_DIMMER_MODULE_INTERFACE_SCL);
    //_wire->begin(IOT_DIMMER_MODULE_INTERFACE_SDA, IOT_DIMMER_MODULE_INTERFACE_SCL, DIMMER_I2C_ADDRESS + 1);
    _wire.setClockStretchLimit(45000);

    // ESP I2C does not support slave mode. Use timer to poll metrics instead
    _timer = Scheduler.addTimer(30e3, true, Dimmer_Base::fetchMetrics);
#endif
    auto dimmer = config._H_GET(Config().dimmer);
    _fadeTime = dimmer.fade_time;
    _onOffFadeTime = dimmer.on_fade_time;
    _temperature = 0;
    _vcc = 0;
    _frequency = 0;
#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if AT_MODE_SUPPORTED
        disable_at_mode(Serial);
        if (IOT_DIMMER_MODULE_BAUD_RATE != KFC_SERIAL_RATE) {
            Serial.flush();
            Serial.begin(IOT_DIMMER_MODULE_BAUD_RATE);
        }
    #endif
    #if SERIAL_HANDLER
        SerialHandler::getInstance().addHandler(onData, SerialHandler::RECEIVE);
    #endif
#endif
}

void Dimmer_Base::_end() {
#if !IOT_DIMMER_MODULE_INTERFACE_UART
    Scheduler.removeTimer(_timer);
#endif
#if IOT_DIMMER_MODULE_INTERFACE_UART
    delete &_wire;
#endif
#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if SERIAL_HANDLER
        SerialHandler::getInstance().removeHandler(onData);
    #endif
        if (IOT_DIMMER_MODULE_BAUD_RATE != KFC_SERIAL_RATE) {
            Serial.flush();
            Serial.begin(KFC_SERIAL_RATE);
        }
    #if AT_MODE_SUPPORTED
        enable_at_mode(Serial);
    #endif
#endif
}

#if IOT_DIMMER_MODULE_INTERFACE_UART

void Dimmer_Base::onData(uint8_t type, const uint8_t *buffer, size_t len) {
    _debug_printf_P(PSTR("Driver_DimmerModule::onData(): length %u\n"), len);
    while(len--) {
        dimmer_plugin._wire.feed(*buffer++);
    }
}

void Dimmer_Base::onReceive(int length) {
    // _debug_printf_P(PSTR("Driver_DimmerModule::onReceive(%u)\n"), length);
    dimmer_plugin._onReceive(length);
}

void Dimmer_Base::_onReceive(int length) {
    auto type = _wire.read();
    _debug_printf_P(PSTR("Driver_DimmerModule::_onReceive(%u): type %02x\n"), length, type);

    if (type == DIMMER_TEMPERATURE_REPORT && length >= 4) {
        uint8_t temperature = _wire.read();
        uint16_t vcc;
        float frequency = 0;
        _wire.readBytes(reinterpret_cast<uint8_t *>(&vcc), sizeof(vcc));
        if (length >= 8) {
            _wire.readBytes(reinterpret_cast<uint8_t *>(&frequency), sizeof(frequency));
        }
        _updateMetrics(temperature, vcc, frequency);
    }
    else if (type == DIMMER_TEMPERATURE_ALERT && length == 3) {
        uint8_t temperature = _wire.read();
        uint8_t max_temperature = _wire.read();
        Logger_error(F("Dimmer temperature alarm triggered: %u > %u"), temperature, max_temperature);
    }
}

#else

void Dimmer_Base::fetchMetrics(EventScheduler::TimerPtr timer) {
    // using dimmer_plugin avoids adding extra static variable to Dimmer_Base
    dimmer_plugin._fetchMetrics();
}


void Dimmer_Base::_fetchMetrics() {
    float temp;
    uint16_t vcc;
    float frequency = 0;

    _wire.beginTransmission(DIMMER_I2C_ADDRESS);
    _wire.write(DIMMER_REGISTER_COMMAND);
    _wire.write(DIMMER_COMMAND_READ_AC_FREQUENCY);
    if (_endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, sizeof(frequency)) == sizeof(frequency)) {
        _wire.readBytes(reinterpret_cast<uint8_t *>(&frequency), sizeof(frequency));
    }

    _wire.beginTransmission(DIMMER_I2C_ADDRESS);
    _wire.write(DIMMER_REGISTER_COMMAND);
    _wire.write(DIMMER_COMMAND_READ_VCC);
    _wire.write(DIMMER_REGISTER_COMMAND);
    _wire.write(DIMMER_COMMAND_READ_INT_TEMP);
    const int len = sizeof(temp) + sizeof(vcc);
    if (_endTransmission() == 0) {
        delay(100);

        if (_wire.requestFrom(DIMMER_I2C_ADDRESS, len) == len) {
            _wire.readBytes(reinterpret_cast<uint8_t *>(&temp), sizeof(temp));
            uint8_t temperature = temp;
            debug_printf("%f %d\n", temp, temperature);
            _wire.readBytes(reinterpret_cast<uint8_t *>(&vcc), sizeof(vcc));

            _updateMetrics(temperature, vcc, frequency);
        }
    }
}

#endif

void Dimmer_Base::writeConfig() {
    auto dimmer = config._H_GET(Config().dimmer);

    _wire.beginTransmission(DIMMER_I2C_ADDRESS);
    _wire.write(DIMMER_REGISTER_OPTIONS);
    if (_endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, 1) == 1) {
        uint8_t options = _wire.read();

        if (dimmer.restore_level) {
            options |= DIMMER_OPTIONS_RESTORE_LEVEL;
        } else {
            options &= ~DIMMER_OPTIONS_RESTORE_LEVEL;
        }

        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_OPTIONS);
        _wire.write(options);
        _wire.write(dimmer.max_temperature);
        _wire.write(reinterpret_cast<const uint8_t *>(&dimmer.on_fade_time), sizeof(dimmer.on_fade_time));
        _wire.write(dimmer.temp_check_int);
        _wire.write(reinterpret_cast<const uint8_t *>(&dimmer.linear_correction), sizeof(dimmer.linear_correction));
        if (_endTransmission() == 0) {
            writeEEPROM();
        }
    }

}

void Dimmer_Base::_printStatus(PrintHtmlEntitiesString &out) {
    if (_temperature) {
        out.printf_P(PSTR("Temperature %.2f" HTML_DEG "C"), _temperature);
    }
    if (_vcc) {
        if (_temperature) {
            out.print(F(", v"));
        } else {
            out.print('V');
        }
        out.printf_P(PSTR("oltage %.3fV"), _vcc / 1000.0);
    }
    if (_frequency) {
        if (_vcc || _temperature) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("AC frequency %.2fHz"), _frequency);
    }
}

void Dimmer_Base::_updateMetrics(float temperature, uint16_t vcc, float frequency) {
    _debug_printf_P(PSTR("Dimmer_Base::_updateMetrics(%u, %u, %.2f%)\n"), temperature, vcc, frequency);
    if (_temperature != temperature || _vcc != vcc || _frequency != frequency) {
        auto client = MQTTClient::getClient();
        if (client) {
            JsonUnnamedObject object(2);
            object.add(JJ(type), JJ(ue));
            auto &events = object.addArray(JJ(events));

            String topic = MQTTClient::formatTopic(-1, F("/metrics/"));
            if (_temperature != temperature) {
                _temperature = temperature;
                auto tempStr = String(_temperature, 2);
                client->publish(topic + F("temperature"), MQTTClient::getDefaultQos(), 1, tempStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_temp"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(tempStr));
            }
            if (_vcc != vcc) {
                _vcc = vcc;
                auto vccStr = String(_vcc / 1000.0, 3);
                client->publish(topic + F("vcc"), MQTTClient::getDefaultQos(), 1, vccStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_vcc"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(vccStr));
            }
            if (_frequency != frequency) {
                _frequency = frequency;
                auto freqStr = String(_frequency, 2);
                client->publish(topic + F("frequency"), MQTTClient::getDefaultQos(), 1, freqStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_frequency"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(freqStr));
            }

            if (events.size()) {
                WsWebUISocket::broadcast(object);
            }

        }
    }
}

void Dimmer_Base::_fade(uint8_t channel, int16_t toLevel, float fadeTime) {
    _debug_printf_P(PSTR("Dimmer_Base::_fade(%u, %u, %f)\n"), channel, toLevel, fadeTime);

    _wire.beginTransmission(DIMMER_I2C_ADDRESS);
    _wire.write(DIMMER_REGISTER_CHANNEL);
    _wire.write(channel);
    _wire.write(reinterpret_cast<const uint8_t *>(&toLevel), sizeof(toLevel));
    _wire.write(reinterpret_cast<const uint8_t *>(&fadeTime), sizeof(fadeTime));
    _wire.write(DIMMER_COMMAND_FADE);
    _endTransmission();
}

void Dimmer_Base::writeEEPROM() {
    _debug_printf_P(PSTR("Dimmer_Base::writeEEPROM()\n"));
    _wire.beginTransmission(DIMMER_I2C_ADDRESS);
    _wire.write(DIMMER_REGISTER_COMMAND);
    _wire.write(DIMMER_COMMAND_WRITE_EEPROM);
    _endTransmission();
}

#if DEBUG_IOT_DIMMER_MODULE
uint8_t Dimmer_Base::_endTransmission() {
    auto result = _wire.endTransmission();
    _debug_printf_P(PSTR("Dimmer_Base::endTransmission(): returned %u\n"), result);
    return result;
}
#endif

void Dimmer_Base::getValues(JsonArray &array) {
    _debug_printf_P(PSTR("Dimmer_Base::getValues()\n"));
    JsonUnnamedObject *obj;

    for (uint8_t i = 0; i < getChannelCount(); i++) {
        obj = &array.addObject();
        PrintString id(F("dimmer_channel%u"), i);
        obj->add(JJ(id), id);
        obj->add(JJ(value), getChannel(i));
        obj->add(JJ(state), getChannelState(i));
    }

    obj = &array.addObject();
    obj->add(JJ(id), F("dimmer_temp"));
    obj->add(JJ(state), _temperature != 0);
    obj->add(JJ(value), JsonNumber(_temperature, 2));

    obj = &array.addObject();
    obj->add(JJ(id), F("dimmer_vcc"));
    obj->add(JJ(state), _vcc != 0);
    obj->add(JJ(value), JsonNumber(_vcc / 1000.0, 3));

    obj = &array.addObject();
    obj->add(JJ(id), F("dimmer_frequency"));
    obj->add(JJ(state), _frequency != 0);
    obj->add(JJ(value), JsonNumber(_frequency, 2));
}

void Dimmer_Base::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) {

    _debug_printf_P(PSTR("Dimmer_Base::setValue %s\n"), id.c_str());

    auto ptr = id.c_str();
    if (strncmp_P(ptr, PSTR("dimmer_channel"), 14) == 0) {
        ptr += 14;
        uint8_t channel = *ptr - '0';

        _debug_printf_P(PSTR("Dimmer_Base::setValue channel %d hasValue %d value %d hasState %d state %d\n"), channel, hasValue, value.toInt(), hasState, state);

        if (channel < getChannelCount()) {
            if (hasState) {
                if (state) {
                    on(channel);
                } else {
                    off(channel);
                }
            }
            if (hasValue) {
                setChannel(channel, value.toInt());
            }
        }
    }
}
