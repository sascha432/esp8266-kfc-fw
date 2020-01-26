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
Dimmer_Base::Dimmer_Base() : _serial(Serial), _wire(*new SerialTwoWire(Serial)), _wireLocked(false) {
#else
Dimmer_Base::Dimmer_Base() : _wire(config.initTwoWire()) {
#endif
    _version = DIMMER_DISABLED;
}

Dimmer_Base::~Dimmer_Base()
{
#if IOT_DIMMER_MODULE_INTERFACE_UART
    delete &_wire;
#endif
}

void Dimmer_Base::_begin()
{
    _version = 0;
    _debug_println(F("Dimmer_Base::_begin()"));
#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if SERIAL_HANDLER
        _wire.onReadSerial(SerialHandler::serialLoop);
    #endif
    _wire.begin(DIMMER_I2C_ADDRESS + 1);
    _wire.onReceive(Dimmer_Base::onReceive);

    Scheduler.addTimer(2000, false, [this](EventScheduler::TimerPtr timer) { // delay for 2 seconds
        if (_lockWire()) {
            _wire.beginTransmission(DIMMER_I2C_ADDRESS);
            _wire.write(DIMMER_REGISTER_COMMAND);
            _wire.write(DIMMER_COMMAND_FORCE_TEMP_CHECK);
            _endTransmission();
            _unlockWire();
        }
    });

#endif
    readConfig();
    auto dimmer = config._H_GET(Config().dimmer);
    _fadeTime = dimmer.fade_time;
    _onOffFadeTime = dimmer.on_fade_time;
    _vcc = 0;
    _frequency = NAN;
    _internalTemperature = NAN;
    _ntcTemperature = NAN;
#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if AT_MODE_SUPPORTED
        // disable_at_mode(Serial);
        #if IOT_DIMMER_MODULE_BAUD_RATE != KFC_SERIAL_RATE
            Serial.flush();
            Serial.begin(IOT_DIMMER_MODULE_BAUD_RATE);
        #endif
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

void Dimmer_Base::_end()
{
    _debug_println(F("Dimmer_Base::_end()"));

    _version = DIMMER_DISABLED;

#if IOT_DIMMER_MODULE_INTERFACE_UART
    _wire.onReceive(nullptr);
#else
    Scheduler.removeTimer(_timer);
#endif
#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if SERIAL_HANDLER
        SerialHandler::getInstance().removeHandler(onData);
    #endif
    #if IOT_DIMMER_MODULE_BAUD_RATE != KFC_SERIAL_RATE
        Serial.flush();
        Serial.begin(KFC_SERIAL_RATE);
    #endif
    #if AT_MODE_SUPPORTED
        // enable_at_mode(Serial);
    #endif
#endif
}

#if IOT_DIMMER_MODULE_INTERFACE_UART

void Dimmer_Base::onData(uint8_t type, const uint8_t *buffer, size_t len)
{
    // _debug_printf_P(PSTR("Driver_DimmerModule::onData(): length %u\n"), len);
    while(len--) {
        dimmer_plugin._wire.feed(*buffer++);
    }
}

void Dimmer_Base::onReceive(int length)
{
    // _debug_printf_P(PSTR("Driver_DimmerModule::onReceive(%u)\n"), length);
    dimmer_plugin._onReceive(length);
}

void Dimmer_Base::_onReceive(size_t length)
{
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
#if LOGGER
    else if (type == DIMMER_TEMPERATURE_ALERT && length == 3) {
        uint8_t temperature = _wire.read();
        uint8_t max_temperature = _wire.read();
        Logger_error(F("Dimmer temperature alarm triggered: %u > %u"), temperature, max_temperature);
    }
#endif
}

#else

void Dimmer_Base::fetchMetrics(EventScheduler::TimerPtr timer)
{
    if (timer->getDelay() != METRICS_DEFAULT_UPDATE_RATE) {
        timer->changeOptions(METRICS_DEFAULT_UPDATE_RATE, true);
    }
    // using dimmer_plugin avoids adding extra static variable to Dimmer_Base
    dimmer_plugin._fetchMetrics();
}


void Dimmer_Base::_fetchMetrics()
{
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

void Dimmer_Base::readConfig()
{
    auto &dimmer = config._H_W_GET(Config().dimmer);

    if (_lockWire()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_READ_LENGTH);
        _wire.write(11);
        _wire.write(DIMMER_REGISTER_OPTIONS);
        if (_endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, 11) == 11) {
            dimmer.restore_level = (_wire.read() & DIMMER_OPTIONS_RESTORE_LEVEL) ? true : false;
            dimmer.max_temperature = _wire.read();
            _wire.readBytes(reinterpret_cast<uint8_t *>(&dimmer.on_fade_time), sizeof(dimmer.on_fade_time));
            _wire.read();
            _wire.readBytes(reinterpret_cast<uint8_t *>(&dimmer.linear_correction), sizeof(dimmer.linear_correction));

            _wire.beginTransmission(DIMMER_I2C_ADDRESS);
            _wire.write(DIMMER_REGISTER_READ_LENGTH);
            _wire.write(1);
            _wire.write(DIMMER_REGISTER_METRICS_INT);
            if (_endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, 1) == 1) {
                dimmer.metrics_int = _wire.read();
            }
        }
    }
    _unlockWire();
}

void Dimmer_Base::writeConfig()
{
    auto dimmer = config._H_GET(Config().dimmer);

    if (_lockWire()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_READ_LENGTH);
        _wire.write(1);
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
            _wire.write(options);                                                                                       // DIMMER_REGISTER_OPTIONS             0xA2 - byte
            _wire.write(dimmer.max_temperature);                                                                        // DIMMER_REGISTER_MAX_TEMP            0xA3 - byte
            _wire.write(reinterpret_cast<const uint8_t *>(&dimmer.on_fade_time), sizeof(float));                        // DIMMER_REGISTER_FADE_IN_TIME        0xA4 - float
            _wire.write(2);                                                                                             // DIMMER_REGISTER_TEMP_CHECK_INT      0xA8 - byte
            _wire.write(reinterpret_cast<const uint8_t *>(&dimmer.linear_correction), sizeof(float));                   // DIMMER_REGISTER_LC_FACTOR           0xA9 - float
            if (_endTransmission() == 0) {
                _wire.beginTransmission(DIMMER_I2C_ADDRESS);
                _wire.write(DIMMER_REGISTER_METRICS_INT);
                _wire.write(dimmer.metrics_int);                                                                        // DIMMER_REGISTER_METRICS_INT         0xB8 - byte
                if (_endTransmission() == 0) {
                    writeEEPROM(true);
                }
            }
        }
    }
    _unlockWire();
}

void Dimmer_Base::_printStatus(Print &output)
{
    PrintHtmlEntitiesString out;
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
    static_cast<PrintHtmlEntitiesString &>(output).setRawOutput(true);
    output.print(out);
    static_cast<PrintHtmlEntitiesString &>(output).setRawOutput(false);
}

void Dimmer_Base::_updateMetrics(uint16_t vcc, float frequency, float internalTemperature, float ntcTemperature)
{
    _debug_printf_P(PSTR("Dimmer_Base::_updateMetrics(%u, %.3f, %.2f, %.2f)\n"), vcc, frequency, internalTemperature, ntcTemperature);
    if (_vcc != vcc || _frequency != frequency || _internalTemperature != internalTemperature || _ntcTemperature != ntcTemperature) {
        auto client = MQTTClient::getClient();
        if (client && client->isConnected()) {
            auto qos = MQTTClient::getDefaultQos();
            JsonUnnamedObject object(2);
            object.add(JJ(type), JJ(ue));
            auto &events = object.addArray(JJ(events));

            if (_internalTemperature != internalTemperature) {
                _internalTemperature = internalTemperature;
                auto tempStr = String(_internalTemperature, 2);
                client->publish(_getMetricsTopics(0), qos, 1, tempStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_int_temp"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(tempStr));
            }
            if (_ntcTemperature != ntcTemperature) {
                _ntcTemperature = ntcTemperature;
                auto tempStr = String(_ntcTemperature, 2);
                client->publish(_getMetricsTopics(1), qos, 1, tempStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_ntc_temp"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(tempStr));
            }
            if (_vcc != vcc) {
                _vcc = vcc;
                auto vccStr = String(_vcc / 1000.0, 3);
                client->publish(_getMetricsTopics(2), qos, 1, vccStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_vcc"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(vccStr));
            }
            if (_frequency != frequency) {
                _frequency = frequency;
                auto freqStr = String(_frequency, 2);
                client->publish(_getMetricsTopics(3), qos, 1, freqStr);
                auto &value = events.addObject(3);
                value.add(JJ(id), F("dimmer_frequency"));
                value.add(JJ(state), true);
                value.add(JJ(value), JsonNumber(freqStr));
            }

            if (events.size()) {
                WsWebUISocket::broadcast(WsWebUISocket::getSender(), object);
            }

        }
    }
}

void Dimmer_Base::_fade(uint8_t channel, int16_t toLevel, float fadeTime)
{
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
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    _setDimmingLevels();
#endif
}

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT

void Dimmer_Base::_setDimmingLevels() {
    // this only works if all channels have about the same load
    float level = 0;
    for(uint8_t i = 0; i < getChannelCount(); i++) {
        level += getChannel(i);
    }
    level = level / (float)(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * getChannelCount());
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTTSensor::HLW8012 || sensor->getType() == MQTTSensor::HLW8032) {
            reinterpret_cast<Sensor_HLW80xx *>(sensor)->setDimmingLevel(level);
        }
    }
}

#endif

void Dimmer_Base::writeEEPROM(bool noLocking)
{
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

String Dimmer_Base::_getMetricsTopics(uint8_t num) const
{
    String topic = MQTTClient::formatTopic(-1, F("/metrics/"));
    switch(num) {
        case 0:
            return topic + F("int_temp");
        case 1:
            return topic + F("ntc_temp");
        case 2:
            return topic + F("vcc");
        case 3:
            return topic + F("frequency");
        case 4:
            return topic + F("power");
    }
    return topic;
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

void Dimmer_Base::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    _debug_printf_P(PSTR("Dimmer_Base::setValue %s\n"), id.c_str());

    if (String_startsWith(id, PSTR("dimmer_channel"))) {
        uint8_t channel = id[14] - '0';
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

void Dimmer_Base::setupWebServer()
{
    _debug_printf_P(PSTR("Dimmer_Base::setupWebServer(): %p\n"), get_web_server_object());
    web_server_add_handler(F("/dimmer_rstfw.html"), Dimmer_Base::handleWebServer);
}

void Dimmer_Base::handleWebServer(AsyncWebServerRequest *request)
{
    if (web_server_is_authenticated(request)) {
        resetDimmerMCU();
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        request->send_P(200, FSPGM(mime_text_plain), SPGM(OK));
    } else {
        request->send(403);
    }
}

void Dimmer_Base::resetDimmerMCU()
{
    digitalWrite(STK500V1_RESET_PIN, LOW);
    pinMode(STK500V1_RESET_PIN, OUTPUT);
    digitalWrite(STK500V1_RESET_PIN, LOW);
    delay(10);
    pinMode(STK500V1_RESET_PIN, INPUT);
}

#endif
