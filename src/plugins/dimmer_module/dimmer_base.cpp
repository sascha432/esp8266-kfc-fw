/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_base.h"
#include <KFCJson.h>
#include <ESPAsyncWebServer.h>
#include <kfc_fw_config.h>
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
    _wire.onReadSerial(SerialHandler::Wrapper::pollSerial);
    _client = &serialHandler.addClient(onData, SerialHandler::EventType::READ);
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
    serialHandler.removeClient(*_client);
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

void Dimmer_Base::onData(Stream &client)
{
    // __LDBG_printf("length=%u", len);
    while(client.available()) {
        // __LDBG_printf("feed=%u '%c'", *buffer, *buffer);
        dimmer_plugin._wire.feed(client.read());
    }
}

void Dimmer_Base::onReceive(int length)
{
    // __LDBG_printf("length=%u", length);
    dimmer_plugin._onReceive(length);
}

void Dimmer_Base::_onReceive(size_t length)
{
    auto type = _wire.read();
    __LDBG_printf("length=%u type=%02x", length, type);

    if (type == DIMMER_METRICS_REPORT && length >= sizeof(dimmer_metrics_t) + 1) {
        dimmer_metrics_t metrics;
         _wire.read(metrics);
        _updateMetrics(metrics);
    }
    else if (type == DIMMER_TEMPERATURE_ALERT && length == 3) {
        uint8_t temperature = _wire.read();
        uint8_t max_temperature = _wire.read();
        PrintString message(F("Dimmer temperature alarm triggered: %u°C > %u°C"), temperature, max_temperature);
        WebUIAlerts_error(message);
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
    _config = Plugins::Dimmer::getConfig();
    _config.config_valid = false;

    for(uint8_t i = 0; i < 3; i++) {

        __LDBG_printf("attempt=%u", i);

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
                _config.config_valid = true;
                _config.fw = cfg;
                _wire.unlock();
                __LDBG_print("read success");
                return;
            }
            else {
                _config.fw = {};
            }
        }
        _wire.unlock();

        if (!can_yield()) {
            break;
        }

        delay(50);
    }
    __LDBG_print("read failed");
    WebUIAlerts_error(F("Reading firmware configuration failed"), AlertMessage::ExpiresType::REBOOT);
}

void Dimmer_Base::writeConfig()
{
    if (!_config.config_valid) { // readConfig() was not successful
        __LDBG_print("invalid config");
        WebUIAlerts_error(F("Cannot write firmware configuration"), AlertMessage::ExpiresType::REBOOT);
        return;
    }

    _config.fw.fade_in_time = _config.fade_time;

    for(uint8_t i = 0; i < 3; i++) {

        __LDBG_printf("attempt=%u", i);

        if (_wire.lock()) {
            _wire.beginTransmission(DIMMER_I2C_ADDRESS);
            _wire.write(DIMMER_REGISTER_CONFIG_OFS);
            _wire.write(_config.fw);
            if (_wire.endTransmission() == 0) {
                writeEEPROM(true);
                _wire.unlock();
                __LDBG_print("write success");
                return;
            }
        }
        _wire.unlock();

        if (!can_yield()) {
            break;
        }

        delay(50);
    }
    __LDBG_print("write failed");
    WebUIAlerts_error(F("Writing firmware configuration failed"), AlertMessage::ExpiresType::REBOOT);
}

void Dimmer_Base::_printStatus(Print &output)
{
    PrintHtmlEntitiesString out;
    auto length = out.length();
    if (_metrics.hasTemp2()) {
        out.printf_P(PSTR("Internal temperature %.2f" PRINTHTMLENTITIES_DEGREE "C"), _metrics.getTemp2());
    }
    if (_metrics.hasTemp()) {
        if (length != out.length()) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("NTC %.2f" PRINTHTMLENTITIES_DEGREE "C"), _metrics.getTemp());
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
    output.print(out);
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
    __LDBG_printf("channel=%u toLevel=%u fadeTime=%f", channel, toLevel, fadeTime);

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

void Dimmer_Base::_forceMetricsUpdate(uint8_t delay)
{
#if IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTTSensor::SensorType::HLW8012 || sensor->getType() == MQTTSensor::SensorType::HLW8032) {
            reinterpret_cast<Sensor_HLW80xx *>(sensor)->setNextMqttUpdate(delay);
        }
    }
#endif
}

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
    __LDBG_printf("noLocking=%d", noLocking);
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

void Dimmer_Base::_getValues(JsonArray &array)
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

void Dimmer_Base::_setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s", id.c_str());

    if (String_startsWith(id, PSTR("dimmer_channel"))) {
        uint8_t channel = id[14] - '0';
        __LDBG_printf("channel=%d hasValue=%d value=%d hasState=%d state=%d", channel, hasValue, value.toInt(), hasState, state);

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
    __LDBG_printf("server=%p", WebServerPlugin::getWebServerObject());
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

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMG, "DIMG", "Get level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMS, "DIMS", "<channel>,<level>[,<time>]", "Set level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMW, "DIMW", "Write EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMR, "DIMR", "Reset ATmega");

void Dimmer_Base::_atModeHelpGenerator(PGM_P name)
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DIMG), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DIMS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DIMW), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DIMR), name);
}

bool Dimmer_Base::_atModeHandler(AtModeArgs &args, const Dimmer_Base &dimmer, int32_t maxLevel)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMW))) {
        writeEEPROM();
        args.ok();
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMG))) {
        for(uint8_t i = 0; i < dimmer.getChannelCount(); i++) {
            args.printf_P(PSTR("%u: %d"), i, getChannel(i));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMR))) {
        args.printf_P(PSTR("Pulling GPIO%u low for 10ms"), STK500V1_RESET_PIN);
        dimmer.resetDimmerMCU();
        args.printf_P(PSTR("GPIO%u set to input"), STK500V1_RESET_PIN);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMS))) {
        if (args.requireArgs(2, 3)) {
            uint8_t channel = args.toInt(0);
            if (channel < dimmer.getChannelCount()) {
                int level = args.toIntMinMax(1, 0, maxLevel);
                float time = args.toFloat(2, -1);
                args.printf_P(PSTR("Set %u: %d (time %.2f)"), channel, level, time);
                setChannel(channel, level, time);
            }
            else {
                args.print(F("Invalid channel"));
            }
        }
        return true;
    }
    return false;
}

#endif
