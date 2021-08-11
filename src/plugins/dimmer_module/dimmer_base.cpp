/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_base.h"
#include "dimmer_module.h"
#include "dimmer_plugin.h"
#include "../src/plugins/sensor/sensor.h"
#include <WebUISocket.h>

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

void Base::begin()
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
    Base::readConfig(_config);

    #if IOT_DIMMER_MODULE_INTERFACE_UART == 0
        // ESP8266 I2C does not support slave mode. Use timer to poll metrics instead
        if (_config.config_valid) {
            auto time = _config.config_valid && _config.fw.report_metrics_max_interval ? _config.fw.report_metrics_max_interval : 10;
            _Timer(_timer).add(Event::seconds(time), true, Base::fetchMetrics);
        }
    #endif
}

void Base::end()
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
            Logger_error(PrintString(F("Dimmer firmware version %u.%u.%u not supported"), config.version.major, config.version.minor, config.version.revision));
        }
        else {
            LoopFunctions::callOnce([this]() {
                _wire.forceTemperatureCheck();
            });
            return;
        }
    }
    else {
        Logger_error(F("Reading firmware configuration failed"));
    }
    // failure
    config.version = {};
}


#if IOT_DIMMER_MODULE_INTERFACE_UART

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
        Logger_error(PrintString(F("Dimmer temperature alarm triggered: %u%s > %u%s"), event.current_temp, SPGM(UTF8_degreeC), event.max_temp, SPGM(UTF8_degreeC)));
    }
}

#endif

void Base::readConfig(ConfigType &config)
{
    auto reader = _wire.getConfigReader();
    _updateConfig(config, reader, reader.readConfig(5, 100));
    // if address does not match member, copy data
    if (&config != &_config) {
        _config = config;
    }
}

void Base::writeConfig(ConfigType &config)
{
    if (!config.version) {
        Logger_error(F("Cannot store invalid firmware configuration"));
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
    auto maxTime = max(_config.lp_fadetime, max(_config.on_fadetime, _config.off_fadetime));
    if (fadeTime > maxTime) {
        fadeTime = maxTime;
    }
    __LDBG_printf("channel=%u to_level=%u fadetime=%f max_time=%f real_to_level=%u", channel, toLevel, fadeTime, maxTime, _calcLevel(toLevel, channel));

    _wire.fadeTo(channel, DIMMER_CURRENT_LEVEL, _calcLevel(toLevel, channel), fadeTime);
    #if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
        _setDimmingLevels();
    #endif
}

void Base::_stopFading(uint8_t channel)
{
    _wire.fadeTo(channel, DIMMER_CURRENT_LEVEL, DIMMER_CURRENT_LEVEL, 0);
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
        #if IOT_SENSOR_HAVE_HLW8012
            auto sensor = SensorPlugin::getSensor<MQTT::SensorType::HLW8012>();
            if (sensor) {
                sensor->setDimmingLevel(level);
            }
        #elif IOT_SENSOR_HAVE_HLW8032
            auto sensor = SensorPlugin::getSensor<MQTT::SensorType::HLW8032>();
            if (sensor) {
                sensor->setDimmingLevel(level);
            }
        #endif
    }

#endif

// ------------------------------------------------------------------------
// status information
// ------------------------------------------------------------------------

void Base::getStatus(Print &out)
{
    // auto &out = static_cast<PrintHtmlEntitiesString &>(output);
    bool written = false;
    if (isValidTemperature(_metrics.metrics.int_temp)) {
        out.printf_P(PSTR("Internal temperature %.1f %s"), static_cast<float>(_metrics.metrics.int_temp), SPGM(UTF8_degreeC));
        written = true;
    }
    if (isValidTemperature(_metrics.metrics.ntc_temp)) {
        if (written) {
            out.print(F(", "));
        }
        written = true;
        out.printf_P(PSTR("NTC %.2f %s"), _metrics.metrics.ntc_temp, SPGM(UTF8_degreeC));
    }
    if (isValidVoltage(_metrics.metrics.vcc)) {
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

void Base::_updateMetrics(const MetricsType &metrics)
{
    auto sensor = getMetricsSensor();
    if (sensor) {
        _metrics = sensor->_updateMetrics(metrics);
    }
}

void Base::_forceMetricsUpdate(uint8_t delay)
{
    #if IOT_SENSOR_HAVE_HLW8012
        auto sensor = SensorPlugin::getSensor<MQTT::SensorType::HLW8012>();
        if (sensor) {
            sensor->setNextMqttUpdate(delay);
        }
    #elif IOT_SENSOR_HAVE_HLW8032
        auto sensor = SensorPlugin::getSensor<MQTT::SensorType::HLW8032>();
        if (sensor) {
            sensor->setNextMqttUpdate(delay);
        }
    #endif
}

float Base::getTransitionTime(int fromLevel, int toLevel, float transitionTimeOverride)
{
    if (transitionTimeOverride < 0) { // negative values are pass through
        return -transitionTimeOverride;
    }
    if (toLevel == 0) {
        __DBG_printf("transition=%.2f off", _config.off_fadetime);
        return _config.off_fadetime;
    }
    if (!isnan(transitionTimeOverride)) {
        auto time = transitionTimeOverride / (abs(fromLevel - toLevel) / (float)DIMMER_MAX_LEVEL); // calculate how much time it takes to dim from 0-100%
        __DBG_printf("transition=%.2f t100=%f", transitionTimeOverride, time);
        return time;
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

void Base::getValues(WebUINS::Events &array)
{
    int on = 0;
    for (uint8_t i = 0; i < getChannelCount(); i++) {
        PrintString id(F("d_chan%u"), i);
        auto value = static_cast<int32_t>(getChannel(i));
        array.append(WebUINS::Values(id, value, true));
        if (getChannelState(i) && value) {
            on = 1;
        }
    }
    array.append(WebUINS::Values(F("group-switch-0"), on, true));
}

void Base::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u has_state=%u value=%s state=%u", id.c_str(), hasValue, hasState, value.c_str(), state);

    if (id == F("group-switch-0")) {
        if (hasValue) {
            auto val = value.toInt() != 0;
            for(uint8_t i = 0; i < getChannelCount(); i++) {
                if (val) {
                    on(i);
                }
                else {
                    off(i);
                }
                publishChannel(i);
                __LDBG_printf("group switch value=%u channel=%u level=%u state=%u", val, i, getChannel(i), getChannelState(i));
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

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "DIM"

#define DIMMER_COMMANDS "reset|weeprom|info|print|write|factory"
#undef DISPLAY

enum class DimmerCommandType {
    INVALID = -1,
    RESET,
    WRITE_EEPROM,
    INFO,
    PRINT,
    WRITE,
    FACTORY
};

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMG, "G", "Get level(s)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMS, "S", "<channel>,<level>[,<time>]", "Set level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMCF, "CF", "<" DIMMER_COMMANDS ">", "Configure dimmer firmware");

ATModeCommandHelpArrayPtr Base::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(DIMG),
        PROGMEM_AT_MODE_HELP_COMMAND(DIMS),
        PROGMEM_AT_MODE_HELP_COMMAND(DIMCF),
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool Base::atModeHandler(AtModeArgs &args, const Base &dimmer, int32_t maxLevel)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMG))) {
        for(uint8_t i = 0; i < dimmer.getChannelCount(); i++) {
            args.print(F("%u: %d (%s)"), i, getChannel(i), getChannelState(i) ? PSTR("on") : PSTR("off"), dimmer_plugin.getChannels().at(i).getStorededBrightness());
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMS))) {
        if (args.requireArgs(2, 3)) {
            uint8_t channel = args.toInt(0);
            if (channel < dimmer.getChannelCount()) {
                int level = args.toIntMinMax(1, 0, maxLevel);
                float time = args.toFloat(2, -1);
                args.print(F("Set %u: %d (time %.2f)"), channel, level, time);
                setChannel(channel, level, time);
            }
            else {
                args.print(F("Invalid channel"));
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMCF))) {
        auto commands = F(DIMMER_COMMANDS);
        auto index = static_cast<DimmerCommandType>(stringlist_find_P(commands, args.get(0), '|'));
        switch(index) {
            case DimmerCommandType::RESET: {
                    args.print(F("Pulling GPIO%u low for 10ms"), STK500V1_RESET_PIN);
                    dimmer.resetDimmerMCU();
                    args.print(F("GPIO%u set to input"), STK500V1_RESET_PIN);
                }
                break;
            case DimmerCommandType::FACTORY: {
                    _wire.restoreFactory();
                    _wire.writeEEPROM();
                    args.ok();
                }
                break;
            case DimmerCommandType::INFO: {
                    _wire.printInfo();
                    args.ok();
                }
                break;
            case DimmerCommandType::PRINT: {
                    _wire.printConfig();
                    args.ok();
                }
                break;
            case DimmerCommandType::WRITE: {
                    _wire.writeConfig();
                    args.ok();
                }
                break;
            case DimmerCommandType::WRITE_EEPROM: {
                    _wire.writeEEPROM();
                    args.ok();
                }
                break;
            case DimmerCommandType::INVALID:
            // default:
                args.print(F("Invalid command: %s: expected: <%s>"), args.toString(0).c_str(), commands);
                break;
        }
        return true;
    }
    return false;
}

#endif
