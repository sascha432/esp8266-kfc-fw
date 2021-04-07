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

using namespace Dimmer;

Base::Base() :
#if IOT_DIMMER_MODULE_INTERFACE_UART
    _wire(Serial)
#else
    _wire(static_cast<TwoWire &>(config.initTwoWire()))
#endif
{
}

void Base::_begin()
{
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
    _wire.onReceive(Base::onReceive);
#else
#endif

    _config = Plugins::Dimmer::getConfig();
    _readConfig(_config);

#if IOT_DIMMER_MODULE_INTERFACE_UART == 0
    // ESP8266 I2C does not support slave mode. Use timer to poll metrics instead
    if (_config.config_valid) {
        auto time = _config.config_valid && _config.fw.report_metrics_max_interval ? _config.fw.report_metrics_max_interval : 10;
        _Timer(_timer).add(Event::seconds(time), true, Base::fetchMetrics);
    }
#endif
}

void Base::_end()
{
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

// ------------------------------------------------------------------------
// configuration and I2C handling
// ------------------------------------------------------------------------

void Base::_updateConfig(ConfigType &config, Dimmer::ConfigReaderWriter &reader, bool status)
{
    if (status) {
        config.version = reader.config().version;
        // config.info = reader.config().info;
        config.cfg = reader.config().config;

        if (config.version.major != DIMMER_FIRMWARE_VERSION_MAJOR || config.version.minor != DIMMER_FIRMWARE_VERSION_MINOR) {
            WebAlerts::Alert::error(PrintString(F("Dimmer firmware version %u.%u.%u not supported"), config.version.major, config.version.minor, config.version.revision));
        }
        else {
            LoopFunctions::callOnce([this]() {
                _wire.forceTemperatureCheck();
            });
            return;
        }
    }
    else {
        WebAlerts::Alert::error(F("Reading firmware configuration failed"));
    }
    // failure
    config.version = {};
}


#if IOT_DIMMER_MODULE_INTERFACE_UART

void Base::onData(Stream &client)
{
    while(client.available()) {
        dimmer_plugin._wire.feed(client.read());
    }
}

void Base::onReceive(int length)
{
    dimmer_plugin._onReceive(length);
}

void Base::_onReceive(size_t length)
{
    auto type = _wire.read();
#ifdef DIMMER_EVENT_RESTART
    if (type == DIMMER_EVENT_RESTART) {
        auto reader = std::shared_ptr<ConfigReaderWriter>(new ConfigReaderWriter(_wire, DIMMER_I2C_ADDRESS));
        reader->readConfig(10, 500, [this, reader](ConfigReaderWriter &config, bool status) {
            if (status) {
                _config.version = config.config().version;
                _config.cfg = config.config().config;
            }
            else {
                _config.version = {};
            }
        }, 100);
    }
    else
#endif
    if (type == DIMMER_EVENT_METRICS_REPORT && length >= sizeof(dimmer_metrics_t) + 1) {
        MetricsType metrics;
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

void Base::fetchMetrics(Event::CallbackTimerPtr timer)
{
    timer->updateInterval(Event::milliseconds(METRICS_DEFAULT_UPDATE_RATE));
    // using dimmer_plugin avoids adding extra static variable to Base
    dimmer_plugin._fetchMetrics();
}

void Base::_fetchMetrics()
{
    auto metrics = _wire.getMetrics();
    if (metrics) {
        _updateMetrics(static_cast<dimmer_metrics_t>(metrics));
    }
}

#endif

void Base::_readConfig(ConfigType &config)
{
    auto reader = _wire.getConfigReader();
    _updateConfig(config, reader, reader.readConfig(5, 100));
    // if address does not match member, copy data
    if (&config != &_config) {
        _config = config;
    }
}

void Base::_writeConfig(ConfigType &config)
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
    auto writer = _wire.getConfigWriter();
    writer.config().version = config.version;
    writer.config().config = config.cfg;

    writer.storeConfig(3, 50);
}


void Base::_fade(uint8_t channel, int16_t toLevel, float fadeTime)
{
    __LDBG_printf("channel=%u toLevel=%u fadeTime=%f realToLevel=%u", channel, toLevel, fadeTime, _calcLevel(toLevel, channel));

    _wire.fadeTo(channel, DIMMER_CURRENT_LEVEL, _calcLevel(toLevel, channel), fadeTime);
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    _setDimmingLevels();
#endif
}

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT

void Base::_setDimmingLevels()
{
    // this only works if all channels have about the same load
    float level = 0;
    for(uint8_t i = 0; i < getChannelCount(); i++) {
        level += getChannel(i);
    }
    level = level / (float)(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * getChannelCount());
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTT::SensorType::HLW8012 || sensor->getType() == MQTT::SensorType::HLW8032) {
            reinterpret_cast<Sensor_HLW80xx *>(sensor)->setDimmingLevel(level);
        }
    }
}

#endif

// ------------------------------------------------------------------------
// status information
// ------------------------------------------------------------------------

void Base::_printStatus(Print &output)
{
    auto &out = static_cast<PrintHtmlEntitiesString &>(output);
    bool written = false;
    if (isValidTemperature(_metrics.metrics.int_temp)) {
        out.printf_P(PSTR("Internal temperature %.1f" PRINTHTMLENTITIES_DEGREE "C"), static_cast<float>(_metrics.metrics.int_temp));
        written = true;
    }
    if (isValidTemperature(_metrics.metrics.ntc_temp)) {
        if (written) {
            out.print(F(", "));
        }
        written = true;
        out.printf_P(PSTR("NTC %.2f" PRINTHTMLENTITIES_DEGREE "C"), _metrics.metrics.ntc_temp);
    }
    if (isValidVoltage(_metrics.metrics.vcc)) {
        if (written) {
            out.print(F(", "));
        }
        written = true;
         out.printf_P(PSTR("VCC %.3fV"), _metrics.metrics.vcc / 1000.0);
     }  if (!isnan(_metrics.metrics.frequency) && _metrics.metrics.frequency) {
        if (written) {
            out.print(F(", "));
        }
        out.printf_P(PSTR("AC frequency %.2fHz"), _metrics.metrics.frequency);
    }
    if (_config.version) {
        out.printf_P(PSTR(HTML_S(br) "Firmware Version %u.%u.%u"), _config.version.major, _config.version.minor, _config.version.revision);
    }
}

void Base::_updateMetrics(const MetricsType &metrics)
{
    auto sensor = getMetricsSensor();
    if (sensor) {
        _metrics = sensor->_updateMetrics(metrics);
    }
}

void Base::_forceMetricsUpdate(uint8_t delay)
{
#if IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTT::SensorType::HLW8012 || sensor->getType() == MQTT::SensorType::HLW8032) {
            reinterpret_cast<Sensor_HLW80xx *>(sensor)->setNextMqttUpdate(delay);
        }
    }
#endif
}

Sensor_DimmerMetrics *Base::getMetricsSensor() const
{
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTT::SensorType::DIMMER_METRICS) {
            return reinterpret_cast<Sensor_DimmerMetrics *>(sensor);
        }
    }
    return nullptr;
}

float Base::getTransitionTime(int fromLevel, int toLevel, float transitionTimeOverride)
{
    if (transitionTimeOverride < 0) { // negative values are pass through
        return -transitionTimeOverride;
    }
    if (!isnan(transitionTimeOverride)) {
        auto time = transitionTimeOverride / (abs(fromLevel - toLevel) / (float)DIMMER_MAX_LEVEL); // calculate how much time it takes to dim from 0-100%
        __DBG_printf("transition=%.2f t100=%f", transitionTimeOverride, time);
        return time;
    }
    if (toLevel == 0) {
        __DBG_printf("transition=%.2f off", _config.off_fadetime);
        return _config.off_fadetime;
    }
    if (fromLevel == 0 && toLevel) {
        __DBG_printf("transition=%.2f on", _config.on_fadetime);
        return _config.on_fadetime;
    }
    __DBG_printf("transition=%.2f fade", _config.lp_fadetime);
    return _config.lp_fadetime;
}


// ------------------------------------------------------------------------
// WebUI/MQTT
// ------------------------------------------------------------------------

void Base::_getValues(WebUINS::Events &array)
{
    bool on = false;
    for (uint8_t i = 0; i < getChannelCount(); i++) {
        PrintString id(F("d_chan%u"), i);
        auto value = static_cast<int32_t>(getChannel(i));
        array.append(WebUINS::Values(id, value));
        if (getChannelState(i) && value) {
            on = true;
        }
    }
    array.append(WebUINS::Values(F("group-switch-0"), on ? 1 : 0));
}

void Base::_setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u has_state=%u value=%s state=%u", id.c_str(), hasValue, hasState, value.c_str(), state);

    if (id == F("group-switch-0")) {
        if (hasValue) {
            auto val = value.toInt() != 0;
            for(uint8_t i = 0; i < getChannelCount(); i++) {
                if (val) {
                    on(i);
                } else {
                    off(i);
                }
                publishChannel(i);
                __DBG_printf("group switch value=%u channel=%u level=%u state=%u", val, i, getChannel(i), getChannelState(i));
            }
        }
    }
    else if (id.startsWith(F("d_chan"))) {
        uint8_t channel = id[6] - '0';
        int val = value.toInt();
        __LDBG_printf("channel=%d has_value=%d value=%d has_state=%d state=%d", channel, hasValue, val, hasState, state);

        if (channel < getChannelCount()) {
            if (hasValue) {
                hasState = true;
                state = val != 0;
            }
            if (hasState) {
                if (state && !getChannelState(channel)) {
                    on(channel);
                }
                else if (!state && getChannelState(channel)) {
                    off(channel);
                }
            }
            if (hasValue) {
                setChannel(channel, val);
            }
        }
    }
}

// ------------------------------------------------------------------------
// web server and AT mode
// ------------------------------------------------------------------------

void Base::setupWebServer()
{
    __LDBG_printf("server=%p", WebServer::Plugin::getWebServerObject());
    WebServer::Plugin::addHandler(F("/dimmer-reset-fw"), Base::handleWebServer);
}

void Base::handleWebServer(AsyncWebServerRequest *request)
{
    if (WebServer::Plugin::getInstance().isAuthenticated(request) == true) {
        resetDimmerMCU();
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        auto response = request->beginResponse_P(200, FSPGM(mime_text_plain), SPGM(OK));
        httpHeaders.setResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void Base::resetDimmerMCU()
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

void Base::_atModeHelpGenerator(PGM_P name)
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DIMG), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DIMS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DIMW), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DIMR), name);
}

bool Base::_atModeHandler(AtModeArgs &args, const Base &dimmer, int32_t maxLevel)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMW))) {
        _wire.writeEEPROM();
        args.ok();
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMG))) {
        for(uint8_t i = 0; i < dimmer.getChannelCount(); i++) {
            args.printf_P(PSTR("%u: %d (%s)"), i, getChannel(i), getChannelState(i) ? PSTR("on") : PSTR("off"), dimmer_plugin.getChannels().at(i).getStorededBrightness());
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
