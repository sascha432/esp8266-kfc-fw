/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_base.h"
#include "dimmer_module.h"
#include "dimmer_plugin.h"
#include <WebUISocket.h>
#include <async_web_response.h>
#include "../src/plugins/sensor/sensor.h"

#if DEBUG_IOT_DIMMER_MODULE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace Dimmer;

Base::Base() :
    #if IOT_DIMMER_MODULE_INTERFACE_UART
        _wire(Serial)
    #else
        _wire(static_cast<TwoWire &>(config.initTwoWire()))
    #endif
    #if IOT_DIMMER_HAS_COLOR_TEMP
        , _color(this)
    #endif
    #if IOT_DIMMER_HAS_RGB
        , _rgb(this)
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
        _wire.setTimeout(1000);
        _wire.begin(DIMMER_I2C_MASTER_ADDRESS);
        _wire.onReceive(Base::onReceive);
    #endif

    readConfig(_getConfig());

    #if IOT_DIMMER_MODULE_INTERFACE_UART == 0
        // ESP8266 I2C does not support slave mode. Use timer to poll metrics instead
        if (_config.config_valid) {
            auto time = _config.config_valid && _config.fw.report_metrics_max_interval ? _config.fw.report_metrics_max_interval : 10;
            _Timer(_timer).add(Event::seconds(time), true, Base::fetchMetrics);
        }
    #endif

    #if IOT_DIMMER_HAS_COLOR_TEMP
        _color.begin();
    #endif
}

void Base::end()
{
    auto &_channels = getChannels();
    for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
        _channels[i].end();
    }

    #if IOT_DIMMER_HAS_COLOR_TEMP
        _color.end();
    #endif

    _config.invalidate();

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
        _Timer(_timer).remove();
    #endif
}

// ------------------------------------------------------------------------
// configuration and I2C handling
// ------------------------------------------------------------------------

void Base::_updateConfig(ConfigType &config, const Dimmer::ConfigReaderWriter &reader, bool status)
{
    if (status) {
        config._version = reader.config().version;
        config._info = reader.config().info;
        config._firmwareConfig = reader.config().config;

        if (config._version.major != DIMMER_FIRMWARE_VERSION_MAJOR || config._version.minor != DIMMER_FIRMWARE_VERSION_MINOR) {
            Logger_error(PrintString(F("Dimmer firmware version %u.%u.%u not supported"), config._version.major, config._version.minor, config._version.revision));
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
    config.invalidate();
}


#if IOT_DIMMER_MODULE_INTERFACE_UART

    void Base::_onReceive(size_t length)
    {
        auto type = _wire.read();
        #ifdef DIMMER_EVENT_RESTART
            if (type == DIMMER_EVENT_RESTART) {
                auto reader = std::shared_ptr<ConfigReaderWriter>(new ConfigReaderWriter(_wire, DIMMER_I2C_ADDRESS));
                reader->readConfig(10, 500, [this, reader](ConfigReaderWriter &config, bool status) {
                    _updateConfig(_config, *reader, status);
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

bool Base::readConfig(ConfigType &config)
{
    auto reader = _wire.getConfigReader();
    _updateConfig(config, reader, reader.readConfig(3, 250));
    // if address does not match member, copy data
    if (&config != &_config) {
        _config = config;
    }
    _config._base = Plugins::Dimmer::getConfig();
    #if IOT_ATOMIC_SUN_V2
        _color._channel_ww1 = _config._base.channel_mapping[0];
        _color._channel_ww2 = _config._base.channel_mapping[1];
        _color._channel_cw1 = _config._base.channel_mapping[2];
        _color._channel_cw2 = _config._base.channel_mapping[3];
    #endif
    return _config;
}

bool Base::writeConfig(ConfigType &config)
{
    if (!config) {
        Logger_error(F("Cannot store invalid firmware configuration"));
        return false;
    }
    config._firmwareConfig.fade_in_time = config._base.on_fadetime;
    // if address does not match member, copy data
    if (&config != &_config) {
        _config = config;
    }
    __DBG_printf("getConfigWriter");
    auto writer = _wire.getConfigWriter();
    writer.config().version = config._version;
    writer.config().info = config._info;
    writer.config().config = config._firmwareConfig;
    if (!writer.storeConfig(3, 250)) {
        Logger_error(F("Error while writing firmware configuration"));
        return false;
    }
    return true;
}

void Base::_fade(uint8_t channel, int16_t toLevel, float fadeTime)
{
    auto maxTime = std::max(_config._base._getFadeTime(), std::max(_config._base.on_fadetime, _config._base.off_fadetime));
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
        uint32_t levelSum = 0;
        for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            levelSum += getChannel(i);
        }
        auto level = levelSum / static_cast<float>(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * IOT_DIMMER_MODULE_CHANNELS);
        auto sensor = SensorPlugin::getSensor<Sensor_HLW80xx::kSensorType>();
        if (sensor) {
            sensor->setDimmingLevel(level);
        }
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
        written = true;
        out.printf_P(PSTR("AC frequency %.2fHz"), _metrics.metrics.frequency);
    }
    if (_config) {
        out.printf_P(PSTR(HTML_S(br) "Firmware Version %u.%u.%u"), _config._version.major, _config._version.minor, _config._version.revision);
    }
    else {
        out.print(F(HTML_S(br) "Failed to read firmware configuration"));
    }
}

void Base::_updateMetrics(const MetricsType &metrics)
{
    auto sensor = getMetricsSensor();
    if (sensor) {
        // update sensor metrics and restore copy
        _metrics = sensor->_updateMetrics(metrics);
    }
}

#if IOT_SENSOR_HAVE_HLW8012

    void Base::_forceMetricsUpdate(uint8_t delay)
    {
        auto sensor = SensorPlugin::getSensor<Sensor_HLW80xx::kSensorType>();
        if (sensor) {
            sensor->setNextMqttUpdate(delay);
        }
    }

#endif

float Base::getTransitionTime(int fromLevel, int toLevel, float transitionTimeOverride)
{
    if (transitionTimeOverride < 0) { // negative values are pass through
        return -transitionTimeOverride;
    }
    if (toLevel == 0) {
        __LDBG_printf("transition=%.2f off", _config._base.off_fadetime);
        return _config._base.off_fadetime;
    }
    if (!isnan(transitionTimeOverride)) {
        auto time = transitionTimeOverride / (static_cast<float>(abs(fromLevel - toLevel)) / static_cast<float>(DIMMER_MAX_LEVEL)); // calculate how much time it takes to dim from 0-100%
        __LDBG_printf("transition=%.2f t100=%f", transitionTimeOverride, time);
        return time;
    }
    if (fromLevel == 0 && toLevel) {
        __LDBG_printf("transition=%.2f on", _config._base.on_fadetime);
        return _config._base.on_fadetime;
    }
    __LDBG_printf("transition=%.2f fade", _config._base._getFadeTime());
    return _config._base._getFadeTime();
}


// ------------------------------------------------------------------------
// WebUI/MQTT
// ------------------------------------------------------------------------

void Base::getValues(WebUINS::Events &array)
{
    int on = 0;
    auto &_channels = getChannels();
    for (uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
        PrintString id(F("d-ch%u"), i);
        auto value = _channels[i].getLevel();
        array.append(WebUINS::Values(id, value, true));
        if (_channels[i].getOnState() && value) {
            on = 1;
        }
    }
    array.append(WebUINS::Values(F("group-switch-0"), on, true));

    #if IOT_DIMMER_HAS_COLOR_TEMP
        _color.getValues(array);
    #endif
    #if IOT_DIMMER_HAS_RGB
        _rgb.getValues(array);
    #endif
}

void Base::_setOnOffState(bool val)
{
    #if IOT_DIMMER_HAS_COLOR_TEMP
        auto &_channels = getChannels();
        for(int i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            if (val) {
                _channels[i]._set(_channels[i]._storedBrightness, NAN, false);
            }
            else {
                _channels[i].setStoredBrightness(_channels[i]._brightness);
                _channels[i]._set(0, NAN, false);
            }
        }
        _wire.writeEEPROM();

        auto color = _color._color;
        _color._channelsToBrightness();
        if (!val) {
            _color._color = color; // keep color when turning off
        }
        _color._publish();
    #else
        for(int i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            if (val) {
                on(i);
            }
            else {
                off(i);
            }
        }
    #endif
    #if IOT_DIMMER_HAS_RGB
        _rgb._publish();
    #endif
}

void Base::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u has_state=%u value=%s state=%u", id.c_str(), hasValue, hasState, value.c_str(), state);

    if (id == F("group-switch-0")) {
        if (hasValue) {
            auto val = value.toInt() != 0;
            _setOnOffState(val);
        }
    }
    else if (id.startsWith(F("d-ch"))) {
        uint8_t channel = id[4] - '0';
        int val = value.toInt();
        __LDBG_printf("channel=%d has_value=%d value=%d has_state=%d state=%d", channel, hasValue, val, hasState, state);

        if (channel < IOT_DIMMER_MODULE_CHANNELS) {
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
            #if IOT_DIMMER_HAS_COLOR_TEMP
                _color._publish();
            #endif
            #if IOT_DIMMER_HAS_RGB
                _rgb._publish();
            #endif
        }
    }
    else {
        #if IOT_DIMMER_HAS_COLOR_TEMP
            _color.setValue(id, value, hasValue, state, hasState);
        #endif
        #if IOT_DIMMER_HAS_RGB
            _rgb.setValue(id, value, hasValue, state, hasState);
        #endif
    }
}

// ------------------------------------------------------------------------
// web server and AT mode
// ------------------------------------------------------------------------

class AsyncDimmerResponse : public AsyncFillBufferCallbackResponse {
public:
    using Callback = std::function<bool (AsyncDimmerResponse &)>;

public:
    AsyncDimmerResponse(const Callback &callback, uint32_t timeout = 2000) :
        AsyncFillBufferCallbackResponse([this](bool *async, bool fillBuffer, AsyncFillBufferCallbackResponse *response) {
            if (*async) {
                if (fillBuffer) {
                    if (_callback(*reinterpret_cast<AsyncDimmerResponse *>(response))) {
                        AsyncFillBufferCallbackResponse::finished(async, response);
                    }
                    else {
                        if (millis() - _start > _timeout) {
                            response->getBuffer() = String(F("{\"error\":\"Timeout while processing request\"}"));
                            AsyncFillBufferCallbackResponse::finished(async, response);
                        }
                    }
                }
            }
        }),
        _callback(callback),
        _start(millis()),
        _timeout(timeout)
    {
        _contentType = FSPGM(mime_application_json);
    }

private:
    Callback _callback;
    uint32_t _start;
    uint32_t _timeout;
};

uint8_t Base::_getChannelFrom(AsyncWebServerRequest *request)
{
    // get and validate channel
    auto channelParam = request->getParam(F("channel"));
    if (!channelParam) {
        __DBG_printf("channel missing");
        WebServer::Plugin::send(400, request);
        return 0xff;
    }
    auto channel = static_cast<uint8_t>(channelParam->value().toInt());
    if (channel >= DIMMER_CHANNEL_COUNT) {
        __DBG_printf("invalid channel=%u", channel);
        WebServer::Plugin::send(400, request);
        return 0xff;
    }
    return channel;
}

void Base::handleWebServer(AsyncWebServerRequest *request)
{
    if (WebServer::Plugin::getInstance().isAuthenticated(request) == true) {
        bool read;
        auto type = request->getParam(F("type"));
        if (!type) {
            __LDBG_printf("type missing");
            WebServer::Plugin::send(405, request);
            return;
        }
        // ----------------------------------------------------------------
        if (type->value() == F("reset")) {
            resetDimmerMCU();
            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();
            auto response = request->beginResponse_P(200, FSPGM(mime_text_plain), SPGM(OK));
            httpHeaders.setResponseHeaders(response);
            WebServer::Plugin::send(request, response);
        }
        // ----------------------------------------------------------------
        else if ((read = (type->value() == F("read-config"))) || type->value() == F("write-config")) {
            auto redirect = request->getParam(F("redirect"));
            if (!redirect) {
                __LDBG_printf("redirect missing");
                WebServer::Plugin::send(410, request);
                return;
            }
            auto data = std::shared_ptr<bool>(new bool(false));
            if (!data) {
                WebServer::Plugin::send(503, request);
                return;
            }
            LoopFunctions::callOnce([read, data]() {
                if (read) {
                    if (!dimmer_plugin.readConfig(dimmer_plugin._getConfig())) {
                        __DBG_printf("failed to read configuration");
                        return;
                    }
                }
                else {
                    if (!dimmer_plugin.writeConfig(dimmer_plugin._getConfig())) {
                        __DBG_printf("failed to write configuration");
                        return;
                    }
                }
                InterruptLock lock;
                *data = true;
            });
            // create async http response that waits for the config to be read
            auto response = new AsyncDimmerResponse([data](AsyncDimmerResponse &response) -> bool {
                // config read?
                if (*data) {
                    response.getBuffer() = String();
                    return true;
                }
                return false;
            });
            response->setCode(302);
            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();
            httpHeaders.add<HttpLocationHeader>(redirect->value());
            httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::ConnectionType::CLOSE);
            httpHeaders.setResponseHeaders(response);
            WebServer::Plugin::send(request, response);
        }
        // ----------------------------------------------------------------
        else if ((read = (type->value() == F("read-ci"))) || type->value() == F("write-ci")) {
            auto channel = _getChannelFrom(request);
            if (channel == 0xff) {
                return;
            }
            auto data = std::shared_ptr<CubicInterpolation>(new CubicInterpolation());
            if (!data) {
                __LDBG_printf("out of memory");
                WebServer::Plugin::send(503, request);
                return;
            }
            // read/write data in main loop
            if (read) {
                LoopFunctions::callOnce([channel, data]() {
                    auto ci = dimmer_plugin._wire.readCubicInterpolation(channel);
                    // copy with locked interrupts
                    InterruptLock lock;
                    *data = ci;
                });
            }
            else {
                auto param = request->getParam(F("data"));
                if (!param) {
                    __DBG_printf("data missing");
                    WebServer::Plugin::send(406, request);
                    return;
                }
                auto &dataStr = param->value();
                if ((dataStr.length() % 4) != 0 || dataStr.length() < 4 || dataStr.length() > CubicInterpolation::size() * 4) {
                    __DBG_printf("invalid data length=%u", dataStr.length());
                    WebServer::Plugin::send(406, request);
                    return;
                }
                auto in = dataStr.c_str();
                auto end = in + dataStr.length();
                auto ptr = data->_data.points;
                char buf[3] = {};
                while(in < end) {
                    buf[0]  = *in++;
                    buf[1]  = *in++;
                    ptr->x = strtoul(buf, nullptr, 16);
                    buf[0]  = *in++;
                    buf[1]  = *in++;
                    ptr->y = strtoul(buf, nullptr, 16);
                    ptr++;
                }
                data->setChannel(channel);
                LoopFunctions::callOnce([read, data]() {
                    if (!dimmer_plugin._wire.writeCubicInterpolation(*data)) {
                        __DBG_printf("failed to store ci for channel=%u", data->_channel);
                    }
                    dimmer_plugin._wire.writeConfig();
                    // reset with locked interrupts
                    InterruptLock lock;
                    *data = CubicInterpolation();
                });
            }
            // create async http response that waits for the data
            auto response = new AsyncDimmerResponse([read, data](AsyncDimmerResponse &response) -> bool {
                // data read/written?
                if (*data == read) {
                    PrintString str = F("{\"error\":null");
                    if (read) {
                        // create json array
                        str.print(F(",\"data\":["));
                        uint8_t maxX = data->_data.points[0].x;
                        for(uint8_t i = 0; i < data->count(); i++) {
                            if (i) {
                                if (data->_data.points[i].x < maxX) {
                                    break;
                                }
                                str.print(',');
                                maxX = data->_data.points[i].x;
                            }
                            str.printf_P(PSTR("{\"x\":%u,\"y\":%u}"), data->_data.points[i].x, data->_data.points[i].y);
                        }
                        str.print(']');
                        // invalidate data
                        data->invalidate();
                    }
                    str.print('}');
                    response.getBuffer() = std::move(str);
                    return true;
                }
                return false;
            });
            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();
            httpHeaders.setResponseHeaders(response);
            WebServer::Plugin::send(request, response);
        }
        // ----------------------------------------------------------------
        else {
            __LDBG_printf("invalid type=%s", type->value().c_str());
            WebServer::Plugin::send(405, request);
        }
    }
    else {
        WebServer::Plugin::send(403, request);
    }
}

