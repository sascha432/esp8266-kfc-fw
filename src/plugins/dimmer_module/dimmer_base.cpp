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
#include "../src/plugins/sensor/sensor.h"
#include "serial_handler.h"
#include "EventTimer.h"

#include "firmware_protocol.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Dimmer_Base::Dimmer_Base() :
#if IOT_DIMMER_MODULE_INTERFACE_UART
    _wire(Serial)
#else
    _wire(static_cast<DimmerTwoWireEx &>(config.initTwoWire()))
#endif
{
}

void Dimmer_Base::_begin()
{
    _debug_println();
    _version = Version();

#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if SERIAL_HANDLER
        _wire.onReadSerial(SerialHandler::serialLoop);
        SerialHandler::getInstance().addHandler(onData, SerialHandler::RECEIVE);
    #endif
    #if AT_MODE_SUPPORTED
        // disable_at_mode(Serial);
        #if IOT_DIMMER_MODULE_BAUD_RATE != KFC_SERIAL_RATE
            Serial.flush();
            Serial.begin(IOT_DIMMER_MODULE_BAUD_RATE);
        #endif
    #endif
    _wire.begin(DIMMER_I2C_ADDRESS + 1);
    _wire.onReceive(Dimmer_Base::onReceive);

    Scheduler.addTimer(2000, false, [this](EventScheduler::TimerPtr timer) { // delay for 2 seconds
        if (_wire.lock()) {
            _wire.beginTransmission(DIMMER_I2C_ADDRESS);
            _wire.write(DIMMER_REGISTER_COMMAND);
            _wire.write(DIMMER_COMMAND_FORCE_TEMP_CHECK);
            _wire.endTransmission();
            _wire.unlock();
        }
    });

#else
    // ESP I2C does not support slave mode. Use timer to poll metrics instead
    _timer.addTimer(2000, true, Dimmer_Base::fetchMetrics);
#endif

    readConfig();
    auto dimmer = config._H_GET(Config().dimmer);
    _fadeTime = dimmer.fade_time;
    _onOffFadeTime = dimmer.on_off_fade_time;

    if (_wire.lock()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DimmerRetrieveVersionLegacy().data());
        if (_wire.endTransmission() == 0) {
            if (_wire.requestFrom(DIMMER_I2C_ADDRESS, _version.size()) == _version.size()) {
                _version.read(_wire);
                if (_version.getMajor() != 2) {
                    __debugbreak_and_panic_printf_P(PSTR("version %u.%u.%u not supported"), DIMMER_VERSION_SPLIT(_version));
                }
            }
        }
        _wire.unlock();
    }
}

void Dimmer_Base::_end()
{
    _debug_println();

    _version = Version();

#if IOT_DIMMER_MODULE_INTERFACE_UART
    _wire.onReceive(nullptr);
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
#else
    _timer.remove();
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
         _wire.read(metrics);
        _updateMetrics(metrics);
    }
    else if (type == DIMMER_TEMPERATURE_ALERT && length == 3) {
        uint8_t temperature = _wire.read();
        uint8_t max_temperature = _wire.read();
        PrintString message(F("Dimmer temperature alarm triggered: %u > %u"), temperature, max_temperature);
        Logger_error(message);
        WebUIAlerts_add(message, AlertMessage::TypeEnum_t::DANGER);
    }
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
    register_mem_metrics_t metrics;

    if (_wire.lock()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_METRICS);
        if (
            (_wire.endTransmission() == 0) &&
            (_wire.requestFrom(DIMMER_I2C_ADDRESS, sizeof(metrics)) == sizeof(metrics)) &&
            (_wire.read(metrics) == sizeof(metrics))
        ) {
            _updateMetrics(dimmer_metrics_t({ 0, metrics.vcc, metrics.frequency, metrics.temperature, metrics.temperature2 }));
        }
        _wire.unlock();
    }
}

#endif

void Dimmer_Base::readConfig()
{
    auto &dimmer = config._H_W_GET(Config().dimmer);
    dimmer.config_valid = false;

    for(uint8_t i = 0; i < 3; i++) {

        _debug_printf_P(PSTR("attempt=%u\n"), i);

        if (_wire.lock()) {
            register_mem_cfg_t cfg;
            _wire.beginTransmission(DIMMER_I2C_ADDRESS);
            _wire.write(DIMMER_REGISTER_READ_LENGTH);
            _wire.write(DIMMER_REGISTER_CONFIG_SZ);
            _wire.write(DIMMER_REGISTER_CONFIG_OFS);
            if (
                (_wire.endTransmission() == 0) &&
                (_wire.requestFrom(DIMMER_I2C_ADDRESS, DIMMER_REGISTER_CONFIG_SZ) == sizeof(cfg)) &&
                (_wire.read(cfg) == sizeof(cfg))
            ) {
                dimmer.config_valid = true;
                dimmer.cfg = cfg;
                _wire.unlock();
                _debug_println(F("read success"));
                return;
            }
            else {
                dimmer.cfg = {};
            }
        }
        _wire.unlock();

        delay(50);
    }
    _debug_println(F("read failed"));
    WebUIAlerts_add(F("Reading firmware configuration failed"), AlertMessage::TypeEnum_t::DANGER, AlertMessage::ExpiresEnum_t::REBOOT);
}

void Dimmer_Base::writeConfig()
{
    auto dimmer = config._H_GET(Config().dimmer);

    if (!dimmer.config_valid) { // readConfig() was not successful
        _debug_println(F("invalid config"));
        WebUIAlerts_add(F("Cannot write firmware configuration"), AlertMessage::TypeEnum_t::DANGER, AlertMessage::ExpiresEnum_t::REBOOT);
        return;
    }

    dimmer.cfg.fade_in_time = dimmer.fade_time;

    for(uint8_t i = 0; i < 3; i++) {

        _debug_printf_P(PSTR("attempt=%u\n"), i);

        if (_wire.lock()) {
            _wire.beginTransmission(DIMMER_I2C_ADDRESS);
            _wire.write(DIMMER_REGISTER_CONFIG_OFS);
            _wire.write(dimmer.cfg);
            if (_wire.endTransmission() == 0) {
                writeEEPROM(true);
                _wire.unlock();
                _debug_println(F("write success"));
                return;
            }
        }
        _wire.unlock();

        delay(50);
    }
    _debug_println(F("write failed"));
    WebUIAlerts_add(F("Writing firmware configuration failed"), AlertMessage::TypeEnum_t::DANGER, AlertMessage::ExpiresEnum_t::REBOOT);
}

void Dimmer_Base::_printStatus(Print &output)
{
    PrintHtmlEntitiesString out;
    auto length = out.length();
    if (_metrics.hasTemp2()) {
        out.printf_P(PSTR("Internal temperature %.2f" HTML_DEG "C"), _metrics.getTemp2());
    }
    if (_metrics.hasTemp()) {
        if (length != out.length()) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("NTC %.2f" HTML_DEG "C"), _metrics.getTemp());
    }
    if (_metrics.hasVCC()) {
        if (length != out.length()) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("VCC %.3fV"), _metrics.getVCC());
    }
    if (_metrics.hasFrequency()) {
        if (length != out.length()) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("AC frequency %.2fHz"), _metrics.getFrequency());
    }
#if AT_MODE_SUPPORTED
    if (_version) {
        out.print(F(HTML_S(br)));

        out.printf_P(PSTR("Firmware Version %u.%u.%u"), DIMMER_VERSION_SPLIT(_version));
    }
#endif
    static_cast<PrintHtmlEntitiesString &>(output).setRawOutput(true);
    output.print(out);
    static_cast<PrintHtmlEntitiesString &>(output).setRawOutput(false);
}

void Dimmer_Base::_updateMetrics(const dimmer_metrics_t &metrics)
{
    auto sensor = getMetricsSensor();
    if (sensor) {
        _metrics = sensor->_updateMetrics(metrics);
    }
}

void Dimmer_Base::_fade(uint8_t channel, int16_t toLevel, float fadeTime)
{
    _debug_printf_P(PSTR("channel=%u toLevel=%u fadeTime=%f\n"), channel, toLevel, fadeTime);

    if (_wire.lock()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DimmerFadeCommand(channel, DIMMER_CURRENT_LEVEL, toLevel, fadeTime).data());
        _wire.endTransmission();
        _wire.unlock();
    }
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    _setDimmingLevels();
#endif
}

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT

void Dimmer_Base::_setDimmingLevels()
{
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

Sensor_DimmerMetrics *Dimmer_Base::getMetricsSensor() const
{
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTTSensor::SensorType::DIMMER_METRICS) {
            return reinterpret_cast<Sensor_DimmerMetrics *>(sensor);
        }
    }
    return nullptr;
}

void Dimmer_Base::writeEEPROM(bool noLocking)
{
    _debug_printf_P(PSTR("noLocking=%d\n"), noLocking);
    if (noLocking || _wire.lock()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_WRITE_EEPROM);
        _wire.endTransmission();
        if (!noLocking) {
            _wire.unlock();
        }
    }
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

    // obj = &array.addObject(3);
    // obj->add(JJ(id), F("dimmer_int_temp"));
    // obj->add(JJ(state), isnan(_internalTemperature));
    // obj->add(JJ(value), JsonNumber(_internalTemperature, 2));

    // obj = &array.addObject(3);
    // obj->add(JJ(id), F("dimmer_ntc_temp"));
    // obj->add(JJ(state), isnan(_ntcTemperature));
    // obj->add(JJ(value), JsonNumber(_ntcTemperature, 2));

    // obj = &array.addObject(3);
    // obj->add(JJ(id), F("dimmer_vcc"));
    // obj->add(JJ(state), _vcc != 0);
    // obj->add(JJ(value), JsonNumber(_vcc / 1000.0, 3));

    // obj = &array.addObject(3);
    // obj->add(JJ(id), F("dimmer_frequency"));
    // obj->add(JJ(state), isnan(_frequency));
    // obj->add(JJ(value), JsonNumber(_frequency, 2));
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
