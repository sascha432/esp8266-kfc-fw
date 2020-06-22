/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_base.h"
#include <KFCJson.h>
#include <ESPAsyncWebServer.h>
#include "WebUISocket.h"
#include "WebUIAlerts.h"
#include "../mqtt/mqtt_client.h"
#if IOT_ATOMIC_SUN_V2
#include "../atomic_sun/atomic_sun_v2.h"
#else
#include "dimmer_module.h"
#endif
#include "serial_handler.h"
#include "EventTimer.h"

#include <dimmer_protocol.h>
#include <dimmer_reg_mem.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Dimmer_Base::Dimmer_Base() :
#if IOT_DIMMER_MODULE_INTERFACE_UART
    _wire(Serial), _wireLocked(false)
#else
    _wire(config.initTwoWire())
#endif
{
    _version = DIMMER_DISABLED;
}

void Dimmer_Base::_begin()
{
    _version = 0;
    _debug_println();
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
    _timer.addTimer(2000, true, Dimmer_Base::fetchMetrics);
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
    _debug_println();

    _version = DIMMER_DISABLED;

#if IOT_DIMMER_MODULE_INTERFACE_UART
    _wire.onReceive(nullptr);
#else
    _timer.remove();
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
    // _debug_printf_P(PSTR("length=%u\n"), len);
    while(len--) {
        // _debug_printf_P(PSTR("feed=%u '%c'\n"), *buffer, *buffer);
        dimmer_plugin._wire.feed(*buffer++);
    }
}

void Dimmer_Base::onReceive(int length)
{
    // _debug_printf_P(PSTR("length=%u\n"), length);
    dimmer_plugin._onReceive(length);
}

void Dimmer_Base::_onReceive(size_t length)
{
    auto type = _wire.read();
    _debug_printf_P(PSTR("length=%u type=%02x\n"), length, type);

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
        register_mem_cfg_t cfg;
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_READ_LENGTH);
        _wire.write(DIMMER_REGISTER_CONFIG_SZ);
        _wire.write(DIMMER_REGISTER_CONFIG_OFS);
        if (_endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, DIMMER_REGISTER_CONFIG_SZ) == sizeof(cfg)) {
            dimmer.restore_level = cfg.bits.restore_level;
            dimmer.max_temperature = cfg.max_temp;
            dimmer.on_fade_time = cfg.fade_in_time;
            // cfg.temp_check_interval;
            dimmer.linear_correction = cfg.linear_correction_factor;
            dimmer.metrics_int = cfg.report_metrics_max_interval;
            // cfg.zero_crossing_delay_ticks;
            // cfg.minimum_on_time_ticks;
            // cfg.adjust_halfwave_time_ticks;
            // cfg.internal_1_1v_ref;
            // cfg.int_temp_offset;
            // cfg.ntc_temp_offset;
            dimmer.metrics_int = cfg.report_metrics_max_interval;
        }
    }
    _unlockWire();
}

void Dimmer_Base::writeConfig()
{
    auto dimmer = config._H_GET(Config().dimmer);

    if (_lockWire()) {
        register_mem_cfg_t cfg;

        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_READ_LENGTH);
        _wire.write(DIMMER_REGISTER_CONFIG_SZ);
        _wire.write(DIMMER_REGISTER_CONFIG_OFS);
        if (
            (_endTransmission() == 0) &&
            (_wire.requestFrom(DIMMER_I2C_ADDRESS, DIMMER_REGISTER_CONFIG_SZ) == DIMMER_REGISTER_CONFIG_SZ) &&
            (_wire.readBytes(reinterpret_cast<uint8_t *>(&cfg), sizeof(cfg)) == DIMMER_REGISTER_CONFIG_SZ)
        ) {

            cfg.bits.restore_level = dimmer.restore_level;
            cfg.max_temp = dimmer.max_temperature;
            cfg.fade_in_time = dimmer.on_fade_time;
            cfg.temp_check_interval = 2;
            cfg.linear_correction_factor = dimmer.linear_correction;
            // cfg.zero_crossing_delay_ticks;
            // cfg.minimum_on_time_ticks;
            // cfg.adjust_halfwave_time_ticks;
            // cfg.internal_1_1v_ref;
            // cfg.int_temp_offset;
            // cfg.ntc_temp_offset;
            cfg.report_metrics_max_interval = dimmer.metrics_int;

            _wire.beginTransmission(DIMMER_I2C_ADDRESS);
            _wire.write(DIMMER_REGISTER_CONFIG_OFS);
            _wire.write(reinterpret_cast<const uint8_t *>(&cfg), sizeof(cfg));
            if (_endTransmission() == 0) {
                writeEEPROM(true);
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
    _debug_printf_P(PSTR("vcc=%u freq=%.3f intTemp=%.2f ntcTemp=%.2f update=%u\n"), vcc, frequency, internalTemperature, ntcTemperature, (_vcc != vcc || _frequency != frequency || _internalTemperature != internalTemperature || _ntcTemperature != ntcTemperature));
    if (_vcc != vcc || _frequency != frequency || _internalTemperature != internalTemperature || _ntcTemperature != ntcTemperature) {
        auto client = MQTTClient::getClient();
        auto qos = MQTTClient::getDefaultQos();
        if (client && !client->isConnected()) {
            client = nullptr;
        }
        JsonUnnamedObject object(2);
        object.add(JJ(type), JJ(ue));
        auto &events = object.addArray(JJ(events));

        if (_internalTemperature != internalTemperature) {
            _internalTemperature = internalTemperature;
            auto tempStr = String(_internalTemperature, 2);
            if (client) {
                client->publish(_getMetricsTopics(0), qos, 1, tempStr);
            }
            auto &value = events.addObject(3);
            value.add(JJ(id), F("dimmer_int_temp"));
            value.add(JJ(state), true);
            value.add(JJ(value), JsonNumber(tempStr));
        }
        if (_ntcTemperature != ntcTemperature) {
            _ntcTemperature = ntcTemperature;
            auto tempStr = String(_ntcTemperature, 2);
            if (client) {
                client->publish(_getMetricsTopics(1), qos, 1, tempStr);
            }
            auto &value = events.addObject(3);
            value.add(JJ(id), F("dimmer_ntc_temp"));
            value.add(JJ(state), true);
            value.add(JJ(value), JsonNumber(tempStr));
        }
        if (_vcc != vcc) {
            _vcc = vcc;
            auto vccStr = String(_vcc / 1000.0, 3);
            if (client) {
                client->publish(_getMetricsTopics(2), qos, 1, vccStr);
            }
            auto &value = events.addObject(3);
            value.add(JJ(id), F("dimmer_vcc"));
            value.add(JJ(state), true);
            value.add(JJ(value), JsonNumber(vccStr));
        }
        if (_frequency != frequency) {
            _frequency = frequency;
            auto freqStr = String(_frequency, 2);
            if (client) {
                client->publish(_getMetricsTopics(3), qos, 1, freqStr);
            }
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

void Dimmer_Base::_fade(uint8_t channel, int16_t toLevel, float fadeTime)
{
    _debug_printf_P(PSTR("channel=%u toLevel=%u fadeTime=%f\n"), channel, toLevel, fadeTime);

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
        if (sensor->getType() == MQTTSensor::SensorType::HLW8012 || sensor->getType() == MQTTSensor::SensorType::HLW8032) {
            reinterpret_cast<Sensor_HLW80xx *>(sensor)->setDimmingLevel(level);
        }
    }
}

#endif

void Dimmer_Base::writeEEPROM(bool noLocking)
{
    _debug_printf_P(PSTR("noLocking=%d\n"), noLocking);
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


uint8_t Dimmer_Base::_endTransmission()
{
#if DEBUG_IOT_DIMMER_MODULE
    return _debug_print_result(_wire.endTransmission());
#else
    return _wire.endTransmission();
#endif
}

void Dimmer_Base::getValues(JsonArray &array)
{
    _debug_println();
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
    _debug_printf_P(PSTR("id=%s\n"), id.c_str());

    if (String_startsWith(id, PSTR("dimmer_channel"))) {
        uint8_t channel = id[14] - '0';
        _debug_printf_P(PSTR("channel=%d hasValue=%d value=%d hasState=%d state=%d\n"), channel, hasValue, value.toInt(), hasState, state);

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
    _debug_printf_P(PSTR("server=%p\n"), WebServerPlugin::getWebServerObject());
    WebServerPlugin::addHandler(F("/dimmer_rstfw.html"), Dimmer_Base::handleWebServer);
}

void Dimmer_Base::handleWebServer(AsyncWebServerRequest *request)
{
    if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
        resetDimmerMCU();
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        auto response = request->beginResponse_P(200, FSPGM(mime_text_plain), SPGM(OK));
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
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
