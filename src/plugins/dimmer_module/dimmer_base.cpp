/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2

#include "dimmer_base.h"
#include <KFCJson.h>
#include <ESPAsyncWebServer.h>
#include "WebUISocket.h"
#include "progmem_data.h"
#include "../mqtt/mqtt_client.h"
#include "dimmer_module.h"
#if IOT_ATOMIC_SUN_V2
#include "../atomic_sun/atomic_sun_v2.h"
#endif
#include "serial_handler.h"
#include "EventTimer.h"

#include "../../trailing_edge_dimmer/src/dimmer_protocol.h"
#include "../../trailing_edge_dimmer/src/dimmer_reg_mem.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART
Dimmer_Base::Dimmer_Base() : _serial(Serial), _wire(*new SerialTwoWire(Serial)) {
    _wireLocked = false;
    _version = 0;
#else
Dimmer_Base::Dimmer_Base() : _wire(config.initTwoWire()) {
#endif
}

Dimmer_Base::~Dimmer_Base() {
}

void Dimmer_Base::_begin() {
#if IOT_DIMMER_MODULE_INTERFACE_UART
    _wire.onReadSerial(SerialHandler::serialLoop);
    _wire.begin(DIMMER_I2C_ADDRESS + 1);
    _wire.onReceive(Dimmer_Base::onReceive);

    Scheduler.addTimer(2e3, false, [this](EventScheduler::TimerPtr timer) { // delay for 2 seconds
        if (_lockWire()) {
            _wire.beginTransmission(DIMMER_I2C_ADDRESS);
            _wire.write(DIMMER_REGISTER_COMMAND);
            _wire.write(DIMMER_COMMAND_FORCE_TEMP_CHECK);
            _endTransmission();
            _unlockWire();
        }
    });

#endif
    auto dimmer = config._H_GET(Config().dimmer);
    _fadeTime = dimmer.fade_time;
    _onOffFadeTime = dimmer.on_fade_time;
    _vcc = 0;
    _frequency = NAN;
    _internalTemperature = NAN;
    _ntcTemperature = NAN;
    // _powerUsage = NAN;
    _metricsTopics[0] = MQTTClient::formatTopic(-1, F("/metrics/int_temp"));
    _metricsTopics[1] = MQTTClient::formatTopic(-1, F("/metrics/ntc_temp"));
    _metricsTopics[2] = MQTTClient::formatTopic(-1, F("/metrics/vcc"));
    _metricsTopics[3] = MQTTClient::formatTopic(-1, F("/metrics/frequency"));
    _metricsTopics[4] = MQTTClient::formatTopic(-1, F("/metrics/power"));
#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if AT_MODE_SUPPORTED
        // disable_at_mode(Serial);
        if (IOT_DIMMER_MODULE_BAUD_RATE != KFC_SERIAL_RATE) {
            Serial.flush();
            Serial.begin(IOT_DIMMER_MODULE_BAUD_RATE);
        }
    #endif
    #if SERIAL_HANDLER
        SerialHandler::getInstance().addHandler(onData, SerialHandler::RECEIVE);
    #endif
#else
    // ESP I2C does not support slave mode. Use timer to poll metrics instead
    Scheduler.addTimer(&_timer, 2e3, true, Dimmer_Base::fetchMetrics);
#endif

    if (_lockWire()) {
        uint16_t version;
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_READ_LENGTH);
        _wire.write((uint8_t)sizeof(version));
        _wire.write(DIMMER_REGISTER_VERSION);
        if (_endTransmission() == 0) {
            if (_wire.requestFrom(DIMMER_I2C_ADDRESS, sizeof(version)) == sizeof(version)) {
                _wire.readBytes(reinterpret_cast<uint8_t *>(&version), sizeof(version));
                _version = version;
            }
        }
        _unlockWire();
    }
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
        // enable_at_mode(Serial);
    #endif
#endif
    for(uint8_t i = 0; i < 5; i++) {
        _metricsTopics[i] = String();
    }
}

#if IOT_DIMMER_MODULE_INTERFACE_UART

void Dimmer_Base::onData(uint8_t type, const uint8_t *buffer, size_t len) {
    // _debug_printf_P(PSTR("Driver_DimmerModule::onData(): length %u\n"), len);
    while(len--) {
        dimmer_plugin._wire.feed(*buffer++);
    }
}

void Dimmer_Base::onReceive(int length) {
    // _debug_printf_P(PSTR("Driver_DimmerModule::onReceive(%u)\n"), length);
    dimmer_plugin._onReceive(length);
}

void Dimmer_Base::_onReceive(size_t length) {
    auto type = _wire.read();
    _debug_printf_P(PSTR("Driver_DimmerModule::_onReceive(%u): type %02x\n"), length, type);

    if (type == DIMMER_METRICS_REPORT && length >= sizeof(dimmer_metrics_t) + 1) {
        dimmer_metrics_t metrics;
        // _wire.readBytes(reinterpret_cast<uint8_t *>(&metrics), sizeof(metrics));
        metrics.temp_check_value = _wire.read();
        _wire.readBytes(reinterpret_cast<uint8_t *>(&metrics.vcc), sizeof(metrics.vcc));
        _wire.readBytes(reinterpret_cast<uint8_t *>(&metrics.frequency), sizeof(metrics.frequency));
        _wire.readBytes(reinterpret_cast<uint8_t *>(&metrics.ntc_temp), sizeof(metrics.ntc_temp));
        _wire.readBytes(reinterpret_cast<uint8_t *>(&metrics.internal_temp), sizeof(metrics.internal_temp));
        _updateMetrics(metrics.vcc, metrics.frequency, metrics.internal_temp, metrics.ntc_temp);
    }
    else if (type == DIMMER_TEMPERATURE_ALERT && length == 3) {
        uint8_t temperature = _wire.read();
        uint8_t max_temperature = _wire.read();
        Logger_error(F("Dimmer temperature alarm triggered: %u > %u"), temperature, max_temperature);
    }
}

#else

void Dimmer_Base::fetchMetrics(EventScheduler::TimerPtr timer) {
    if (timer->getDelay() != METRICS_DEFAULT_UPDATE_RATE) {
        timer->changeOptions(METRICS_DEFAULT_UPDATE_RATE, true);
    }
    // using dimmer_plugin avoids adding extra static variable to Dimmer_Base
    dimmer_plugin._fetchMetrics();
}


void Dimmer_Base::_fetchMetrics() {
    float frequency = NAN;
    float int_temp = NAN, ntc_temp = NAN;
    uint16_t vcc = 0;
    bool hasData = false;

    if (_lockWire()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_AC_FREQUENCY);
        if (_endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, sizeof(frequency)) == sizeof(frequency)) {
            _wire.readBytes(reinterpret_cast<uint8_t *>(&frequency), sizeof(frequency));
            hasData = true;
        }
        _unlockWire();
    }

    if (_lockWire()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_VCC);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_INT_TEMP);
        const int len = sizeof(int_temp) + sizeof(vcc);
        if (_endTransmission() == 0) {
            _unlockWire();

            delay(100);

            if (_lockWire()) {
                _wire.beginTransmission(DIMMER_I2C_ADDRESS);
                _wire.write(DIMMER_REGISTER_READ_LENGTH);
                _wire.write(len);
                _wire.write(DIMMER_REGISTER_TEMP);
                if (_endTransmission() == 0) {
                    if (_wire.requestFrom(DIMMER_I2C_ADDRESS, len) == len) {
                        _wire.readBytes(reinterpret_cast<uint8_t *>(&int_temp), sizeof(int_temp));
                        _wire.readBytes(reinterpret_cast<uint8_t *>(&vcc), sizeof(vcc));
                        hasData = true;
                    }
                }
                _unlockWire();
            }

            if (_lockWire()) {
                _wire.beginTransmission(DIMMER_I2C_ADDRESS);
                _wire.write(DIMMER_REGISTER_COMMAND);
                _wire.write(DIMMER_COMMAND_READ_NTC);
                if (_endTransmission() == 0) {
                    _unlockWire();

                    delay(20);

                    if (_lockWire()) {
                        const int len2 = sizeof(ntc_temp);
                        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
                        _wire.write(DIMMER_REGISTER_READ_LENGTH);
                        _wire.write(len2);
                        _wire.write(DIMMER_REGISTER_TEMP);
                        if (_endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, len2) == len2) {
                            _wire.readBytes(reinterpret_cast<uint8_t *>(&ntc_temp), sizeof(ntc_temp));
                            hasData = true;
                        }
                        _unlockWire();
                    }
                }
                else {
                    _unlockWire();
                }
            }
        }
        else {
            _unlockWire();
        }
    }

    if (hasData) {
        _updateMetrics(vcc, frequency, int_temp, ntc_temp);
    }
}

#endif

void Dimmer_Base::writeConfig() {
    auto dimmer = config._H_GET(Config().dimmer);

    if (_lockWire()) {
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
            _wire.write(dimmer.metrics_int);
            _wire.write(reinterpret_cast<const uint8_t *>(&dimmer.linear_correction), sizeof(dimmer.linear_correction));
            if (_endTransmission() == 0) {
                writeEEPROM(true);
            }
        }
    }
    _unlockWire();
}

void Dimmer_Base::_printStatus(PrintHtmlEntitiesString &out) {
    auto length = out.length();
    if (!isnan(_internalTemperature)) {
        out.printf_P(PSTR("Internal temperature %.2f" HTML_DEG "C"), _internalTemperature);
    }
    if (!isnan(_ntcTemperature)) {
        if (length != out.length()) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("NTC %.2f" HTML_DEG "C"), _ntcTemperature);
    }
    if (_vcc) {
        if (length != out.length()) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("VCC %.3fV"), _vcc / 1000.0);
    }
    if (!isnan(_frequency)) {
        if (length != out.length()) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("AC frequency %.2fHz"), _frequency);
    }
#if AT_MODE_SUPPORTED
    if (_version) {
        out.print(F(HTML_S(br)));
        out.printf_P(PSTR("Firmware Version %u.%u.%u"), _version >> 10, (_version >> 5)  & 0b11111, _version & 0b11111);
    }
#endif
}

void Dimmer_Base::_updateMetrics(uint16_t vcc, float frequency, float internalTemperature, float ntcTemperature) {
    _debug_printf_P(PSTR("Dimmer_Base::_updateMetrics(%u, %.3f, %.2f, %.2f)\n"), vcc, frequency, internalTemperature, ntcTemperature);
    if (_vcc != vcc || _frequency != frequency || _internalTemperature != internalTemperature || _ntcTemperature != ntcTemperature) {
        auto client = MQTTClient::getClient();
        if (client) {
            auto qos = MQTTClient::getDefaultQos();
            JsonUnnamedObject object(2);
            object.add(JJ(type), JJ(ue));
            auto &events = object.addArray(JJ(events));

            if (_internalTemperature != internalTemperature) {
                _internalTemperature = internalTemperature;
                auto tempStr = String(_internalTemperature, 2);
                client->publish(_metricsTopics[0], qos, 1, tempStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_int_temp"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(tempStr));
            }
            if (_ntcTemperature != ntcTemperature) {
                _ntcTemperature = ntcTemperature;
                auto tempStr = String(_ntcTemperature, 2);
                client->publish(_metricsTopics[1], qos, 1, tempStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_ntc_temp"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(tempStr));
            }
            if (_vcc != vcc) {
                _vcc = vcc;
                auto vccStr = String(_vcc / 1000.0, 3);
                client->publish(_metricsTopics[2], qos, 1, vccStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_vcc"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(vccStr));
            }
            if (_frequency != frequency) {
                _frequency = frequency;
                auto freqStr = String(_frequency, 2);
                client->publish(_metricsTopics[3], qos, 1, freqStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_frequency"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(freqStr));
            }
            // if (_powerUsage != powerUsage) {
            //     _powerUsage = powerUsage;
            //     auto powerStr = String(_powerUsage, 1);
            //     client->publish(_metricsTopics[4], qos, 1, powerStr);
            //     auto &value = events.addObject(3);
            //     value.add(JJ(id), F("dimmer_power"));
            //     value.add(JJ(state), true);
            //     value.add(JJ(value), JsonNumber(_powerUsage, 1));
            // }

            if (events.size()) {
                WsWebUISocket::broadcast(WsWebUISocket::getSender(), object);
            }

        }
    }
}

void Dimmer_Base::_fade(uint8_t channel, int16_t toLevel, float fadeTime) {
    _debug_printf_P(PSTR("Dimmer_Base::_fade(%u, %u, %f)\n"), channel, toLevel, fadeTime);

    if (_lockWire()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_CHANNEL);
        _wire.write(channel);
        _wire.write(reinterpret_cast<const uint8_t *>(&toLevel), sizeof(toLevel));
        _wire.write(reinterpret_cast<const uint8_t *>(&fadeTime), sizeof(fadeTime));
        _wire.write(DIMMER_COMMAND_FADE);
        _endTransmission();
        _unlockWire();
    }
}

void Dimmer_Base::writeEEPROM(bool noLocking) {
    _debug_printf_P(PSTR("Dimmer_Base::writeEEPROM()\n"));
    if (noLocking || _lockWire()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_WRITE_EEPROM);
        _endTransmission();
        if (!noLocking) {
            _unlockWire();
        }
    }
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
        obj = &array.addObject(3);
        PrintString id(F("dimmer_channel%u"), i);
        obj->add(JJ(id), id);
        obj->add(JJ(value), getChannel(i));
        obj->add(JJ(state), getChannelState(i));
    }

    obj = &array.addObject(3);
    obj->add(JJ(id), F("dimmer_int_temp"));
    obj->add(JJ(state), isnan(_internalTemperature));
    obj->add(JJ(value), JsonNumber(_internalTemperature, 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("dimmer_ntc_temp"));
    obj->add(JJ(state), isnan(_ntcTemperature));
    obj->add(JJ(value), JsonNumber(_ntcTemperature, 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("dimmer_vcc"));
    obj->add(JJ(state), _vcc != 0);
    obj->add(JJ(value), JsonNumber(_vcc / 1000.0, 3));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("dimmer_frequency"));
    obj->add(JJ(state), isnan(_frequency));
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

void Dimmer_Base::setupWebServer() {
    auto server = get_web_server_object();
    _debug_printf_P(PSTR("Dimmer_Base::setupWebServer(): %p\n"), server);
    if (server) {
        server->on(String(F("/dimmer_rstfw.html")).c_str(), Dimmer_Base::handleWebServer);
    }
}

void Dimmer_Base::handleWebServer(AsyncWebServerRequest *request) {
    if (web_server_is_authenticated(request)) {
        resetDimmerFirmware();
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        request->send_P(200, FSPGM(text_plain), SPGM(OK));
    } else {
        request->send(403);
    }
}

void Dimmer_Base::resetDimmerFirmware() {
    digitalWrite(STK500V1_RESET_PIN, LOW);
    pinMode(STK500V1_RESET_PIN, OUTPUT);
    digitalWrite(STK500V1_RESET_PIN, LOW);
    delay(10);
    pinMode(STK500V1_RESET_PIN, INPUT);
}


#endif
