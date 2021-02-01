/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_base.h"
#include <WebUISocket.h>
#include <WebUIAlerts.h>
#if IOT_ATOMIC_SUN_V2
#include "../src/plugins/atomic_sun/atomic_sun_v2.h"
#else
#include "dimmer_module.h"
#include "dimmer_plugin.h"
#endif
#include "../src/plugins/sensor/sensor.h"
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
    __LDBG_println();

#if IOT_DIMMER_MODULE_INTERFACE_UART
    _client = &serialHandler.addClient(onData, SerialHandler::EventType::READ);
    #if AT_MODE_SUPPORTED
        // disable_at_mode(Serial);
        #if IOT_DIMMER_MODULE_BAUD_RATE != KFC_SERIAL_RATE
            Serial.flush();
            Serial.begin(IOT_DIMMER_MODULE_BAUD_RATE);
        #endif
    #endif
    // delay between request and response is ~25ms + ~450us per raw byte
    _wire.setTimeout(500);
    _wire.begin(DIMMER_I2C_MASTER_ADDRESS);
    _wire.onReceive(Dimmer_Base::onReceive);
#else
#endif

    _config = Plugins::Dimmer::getConfig();
    _readConfig(_config);

#if IOT_DIMMER_MODULE_INTERFACE_UART == 0
    // ESP8266 I2C does not support slave mode. Use timer to poll metrics instead
    auto time = _config.config_valid && _config.fw.report_metrics_max_interval ? _config.fw.report_metrics_max_interval : 10;
    _Timer(_timer).add(Event::seconds(time), true, Dimmer_Base::fetchMetrics);
#endif
}

void Dimmer_Base::_updateConfig(ConfigType &config, Dimmer::GetConfig &reader, bool status)
{
    if (status) {
        config.version = reader.config().version;
        config.info = reader.config().info;
        config.cfg = reader.config().config;

        if (config.version.major != DIMMER_FIRWARE_VERSION_MAJOR || config.version.minor != DIMMER_FIRWARE_VERSION_MINOR) {
            WebAlerts::Alert::error(PrintString(F("Dimmer firmware version %u.%u.%u not supported"), config.version.major, config.version.minor, config.version.revision));
        }
        else {
#if IOT_DIMMER_MODULE_INTERFACE_UART
            LoopFunctions::callOnce([this]() {
                if (_wire.lock()) {
                    _wire.beginTransmission(DIMMER_I2C_ADDRESS);
                    _wire.write(DIMMER_REGISTER_COMMAND);
                    _wire.write(DIMMER_COMMAND_FORCE_TEMP_CHECK);
                    _wire.endTransmission();
                    _wire.unlock();
                }
            });
#endif
            // success
            return;
        }
    }
    else {
        WebAlerts::Alert::error(F("Reading firmware configration failed"));
    }
    // failure
    config.version = {};
}

void Dimmer_Base::_end()
{
    __LDBG_println();

    _config.version = {};

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
    // __DBG_printf("length=%u", len);
    while(client.available()) {
        auto data = client.read();
        // __DBG_printf("feed=%u '%c'", data, data);
        dimmer_plugin._wire.feed(data);
    }
}

void Dimmer_Base::onReceive(int length)
{
    // __DBG_printf("len=%u", length);
    dimmer_plugin._onReceive(length);
}

void Dimmer_Base::_onReceive(size_t length)
{
    auto type = _wire.read();
    __LDBG_printf("length=%u type=%02x", length, type);

    if (type == DIMMER_EVENT_RESTART) {
        auto reader = std::shared_ptr<Dimmer::GetConfig>(new Dimmer::GetConfig(_wire, DIMMER_I2C_ADDRESS));
        reader->readConfig(10, 500, [this, reader](Dimmer::GetConfig &config, bool status) {
            if (status) {
                _config.version = config.config().version;
                _config.info = config.config().info;
                _config.cfg = config.config().config;
            }
            else {
                _config.version = {};
            }
        }, 100);
    }
    else if (type == DIMMER_EVENT_METRICS_REPORT && length >= sizeof(dimmer_metrics_t) + 1) {
        dimmer_metrics_t metrics;
         _wire.read(metrics);
        _updateMetrics(metrics);
    }
    else if (type == DIMMER_EVENT_TEMPERATURE_ALERT && length == 3) {
        dimmer_over_temperature_event_t event;
        _wire.read(event);
        WebAlerts::Alert::error(PrintString(F("Dimmer temperature alarm triggered: %u&deg;C > %u&deg;C"), event.current_temp, event.max_temp));
    }
}

#else

void Dimmer_Base::fetchMetrics(Event::CallbackTimerPtr timer)
{
    timer->updateInterval(Event::milliseconds(METRICS_DEFAULT_UPDATE_RATE));
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

void Dimmer_Base::_readConfig(ConfigType &config)
{
    Dimmer::GetConfig reader(_wire, DIMMER_I2C_ADDRESS);
    _updateConfig(config, reader, reader.readConfig(5, 100));

    // if address does not match member, copy data
    if (&config != &_config) {
        _config = config;
    }
}

void Dimmer_Base::_writeConfig(ConfigType &config)
{
    if (!config.version) {
        WebAlerts::Alert::error(F("Cannot store invalid firmware configuration"), WebAlerts::ExpiresType::REBOOT);
        return;
    }

    config.cfg.fade_in_time = config.on_fadetime;

    // if address does not match member, copy data
    if (&config != &_config) {
        _config = config;
    }

    Dimmer::GetConfig writer(_wire, DIMMER_I2C_ADDRESS);

    writer.config().version = config.version;
    writer.config().info = config.info;
    writer.config().config = config.cfg;

    writer.storeConfig(3, 50);

    //     for(uint8_t i = 0; i < 3; i++) {
    //         uint8_t res1 = 0xff;
    //         bool res2;

    //         __LDBG_IF(
    //             if (i) {
    //                 __LDBG_printf("attempt=%u", i + 1);
    //             }
    //         );

    //         if ((res2 = _wire.lock()) != false) {
    //             _wire.beginTransmission(DIMMER_I2C_ADDRESS);
    //             _wire.write(DIMMER_REGISTER_CONFIG_OFS);
    //             _wire.write(reinterpret_cast<const uint8_t *>(&config.fw), DimmerRetrieveVersionLegacy::getRegisterMemCfgSize(config.fw.version));
    //             if ((res1 = _wire.endTransmission()) == 0) {
    //                 writeEEPROM(true);
    //                 _wire.unlock();
    //                 config.config_valid = true;
    //                 __LDBG_print("write success");
    //                 break;
    //             }
    //             _wire.unlock();
    //         }
    //         config.config_valid = false;

    //         __DBG_printf("trans=%u lock=%u can_yield=%u", res1, res2, can_yield());

    //         if (!can_yield()) {
    //             break;
    //         }

    //         delay(50);
    //     }
    // }

    // if (!config.config_valid) {
    //     __LDBG_print("write failed");
    //     WebAlerts::Alert::error(F("Writing firmware configuration failed"), WebAlerts::ExpiresType::REBOOT);
    // }
}

void Dimmer_Base::_printStatus(Print &output)
{
    auto &out = static_cast<PrintHtmlEntitiesString &>(output);
    bool written = false;
    if (!isnan(_metrics.metrics.int_temp)) {
        out.printf_P(PSTR("Internal temperature %d" PRINTHTMLENTITIES_DEGREE "C"), _metrics.metrics.int_temp);
        written = true;
    }
    if (!isnan(_metrics.metrics.ntc_temp)) {
        if (written) {
            out.print(F(", "));
        }
        written = true;
        out.printf_P(PSTR("NTC %.2f" PRINTHTMLENTITIES_DEGREE "C"), _metrics.metrics.ntc_temp);
    }
    if (_metrics.metrics.vcc) {
        if (written) {
            out.print(F(", "));
        }
        written = true;
        out.printf_P(PSTR("VCC %.3fV"), _metrics.metrics.vcc / 1000.0);
    }
    if (!isnan(_metrics.metrics.frequency) && _metrics.metrics.frequency) {
        if (written) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("AC frequency %.2fHz"), _metrics.metrics.frequency);
    }
    if (_config.version) {
        out.printf_P(PSTR(HTML_S(br) "Firmware Version %u.%u.%u"), _config.version.major, _config.version.minor, _config.version.revision);
    }
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

    Dimmer::FadeCommand::fadeTo(_wire, channel, DIMMER_CURRENT_LEVEL, toLevel, fadeTime, DIMMER_I2C_ADDRESS);
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
    // __LDBG_printf("noLocking=%d", noLocking);
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
    __LDBG_println();
    JsonUnnamedObject *obj;
    bool on = false;

    for (uint8_t i = 0; i < getChannelCount(); i++) {
        obj = &array.addObject(3);
        PrintString id(F("dimmer_channel%u"), i);
        obj->add(JJ(id), id);
        auto value = getChannel(i);
        obj->add(JJ(value), value);
        auto state = getChannelState(i);
        obj->add(JJ(state), state);
        if (state && value) {
            on = true;
        }
    }

    obj = &array.addObject(3);
    obj->add(JJ(id), F("group-switch-0"));
    obj->add(JJ(value), on ? 1 : 0);
    obj->add(JJ(state), true);
}

void Dimmer_Base::_setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s", id.c_str());

    if (String_equals(id, PSTR("group-switch-0"))) {
        if (hasValue) {
            int val = value.toInt();
            for(uint8_t i = 0; i < getChannelCount(); i++) {
                if (val) {
                    on(i);
                } else {
                    off(i);
                }
            }
        }
    }
    else if (String_startsWith(id, PSTR("dimmer_channel"))) {
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
    WebServerPlugin::addHandler(F("/dimmer-reset-fw"), Dimmer_Base::handleWebServer);
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
