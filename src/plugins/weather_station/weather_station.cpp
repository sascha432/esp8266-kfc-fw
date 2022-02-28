/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_WEATHER_STATION

#include "weather_station.h"
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <HeapStream.h>
#include <serial_handler.h>
#include <WebUISocket.h>
#include <AsyncBitmapStreamResponse.h>
#include <GFXCanvasRLEStream.h>
#include <kfc_fw_config.h>
#include "RestAPI.h"
#include "web_server.h"
#include "async_web_response.h"
#include "blink_led_timer.h"
#include "./plugins/sensor/sensor.h"
#include "./plugins/sensor/Sensor_BME280.h"

using Plugins = KFCConfigurationClasses::PluginsType;

// web accesss for screen capture
//
// screen capture
// http://192.168.0.95/images/screen_capture.bmp
// http://192.168.0.56/images/screen_capture.bmp
//
//

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_WEATHER_STATION_WS2812_NUM && __LED_BUILTIN == -1
#error invalid built-in LED
#endif

WeatherStationPlugin ws_plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    WeatherStationPlugin,
    // name
    "weather",
    // friendly name
    "Weather Station",
    // web_templates
    "",
    // config_forms
    "weather",
    // reconfigure_dependencies
    "http",
    PluginComponent::PriorityType::WEATHER_STATION,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

WeatherStationPlugin::WeatherStationPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(WeatherStationPlugin)),
    // _pollTimer(0),
    _httpClient(nullptr)
{
    REGISTER_PLUGIN(this, "WeatherStationPlugin");
}

void WeatherStationPlugin::_sendScreenCaptureBMP(AsyncWebServerRequest *request)
{
    // __LDBG_printf("WeatherStationPlugin::_sendScreenCapture(): is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);

    if (WebServer::Plugin::getInstance().isAuthenticated(request) == true) {
        if (WeatherStationPlugin::_getInstance().lock()) {
            #if 1
                auto response = new AsyncBitmapStreamResponse(*WeatherStationPlugin::_getInstance().getCanvas());
            #else
                __LDBG_IF(
                    auto mem = ESP.getFreeHeap()
                    );
                auto response = new AsyncClonedBitmapStreamResponse(lock.getCanvas().clone());
                __LDBG_IF(
                    auto usage = mem - ESP.getFreeHeap();
                    __DBG_printf("AsyncClonedBitmapStreamResponse memory usage %u", usage);
                );
                plugin.releaseCanvasLock();
            #endif
            HttpHeaders httpHeaders;
            httpHeaders.addNoCache();
            httpHeaders.setResponseHeaders(response);
            request->send(response);
        }
        else {
            request->send(503);
        }
    }
    else {
        request->send(403);
    }
}

void WeatherStationPlugin::_installWebhooks()
{
    __LDBG_printf("server=%p", WebServer::Plugin::getWebServerObject());
    WebServer::Plugin::addHandler(F("/images/screen_capture.bmp"), _sendScreenCaptureBMP);

    #if 0
    WebServer::Plugin::addRestHandler(std::move(WebServer::Plugin::RestHandler(F("/api/ws"), [this](AsyncWebServerRequest *request, WebServerPlugin::RestRequest &rest) -> AsyncWebServerResponse * {
        // TODO replace AsyncJsonResponse
        // add no cache no store headers
        // #error read TODO
        auto response = __DBG_new(AsyncJsonResponse);
        auto &json = response->getJsonObject();
        auto &reader = rest.getJsonReader();

        if (_currentScreen == TEXT_UPDATE) {
            json.add(FSPGM(status), 503);
            json.add(FSPGM(message), F("Firmware update in progress"));
        }
        else if (rest.isUriMatch(FSPGM(message))) {
            auto title = reader.get(FSPGM(title)).getValue();
            auto message = reader.get(FSPGM(message)).getValue();
            auto timeout = reader.get(F("timeout")).getValue().toInt();
            auto confirm = reader.get(F("confirm")).getBooleanValue();

            // default time if not set
            if (timeout == 0) {
                timeout = 60;
            }
            // display until message has been confirmed
            if (confirm == JsonVar::BooleanValueType::TRUE) {
                timeout = 0;
            }

            if (message.length() == 0) {
                json.add(FSPGM(status), 400);
                json.add(FSPGM(message), F("Empty message"));
            }
            else {
                json.add(FSPGM(status), 200);
                json.add(FSPGM(message), AsyncWebServerResponse::responseCodeToString(200));
                displayMessage(title, message, ST77XX_YELLOW, ST77XX_WHITE, timeout);
            }
        }
        else if (rest.isUriMatch(FSPGM(display))) {
            _setScreen(((uint8_t)reader.get(F("screen")).getValue().toInt()) % NUM_SCREENS);
            json.add(FSPGM(status), 200);
            json.add(FSPGM(message), PrintString(F("Screen set to #%u"), _currentScreen));
            redraw();
        }
        else if (rest.isUriMatch(F("backlight"))) {
            auto levelVar = reader.get(F("level"));
            int level = levelVar.getValue().toInt();
            if (levelVar.getType() != JsonBaseReader::JSON_TYPE_INT || level < 0 || level > 1023) {
                json.add(FSPGM(status), 400);
                json.add(FSPGM(message), F("Backlight level out of range (0-1023)"));
            }
            else {
                json.add(FSPGM(status), 200);
                json.add(FSPGM(message), PrintString(F("Backlight level set to %u"), level));
                _fadeBacklight(_backlightLevel, level);
                _backlightLevel = level;
            }
        }
        else if (rest.isUriMatch(FSPGM(sensors))) {
            json.add(FSPGM(status), 200);
            auto &sensors = json.addArray(FSPGM(sensors));
            for(const auto sensor: SensorPlugin::getSensors()) {
                String name;
                StringVector values;
                if (sensor->getSensorData(name, values)) {
                    auto &sensor = sensors.addObject(2);
                    sensor.add(FSPGM(name), name);
                    auto &vals = sensor.addArray(FSPGM(values));
                    for(const auto &value: values) {
                        vals.add(value);
                    }
                }
            }
        }
        else if (rest.isUriMatch(nullptr)) {
            json.add(FSPGM(status), 200);
            json.add(FSPGM(message), AsyncWebServerResponse::responseCodeToString(200));
        }
        else {
            json.add(FSPGM(status), 404);
            json.add(FSPGM(message), AsyncWebServerResponse::responseCodeToString(404));
        }

        return response->finalize();
    })));
    #endif
}

void WeatherStationPlugin::_readConfig()
{
    _config = Plugins::WeatherStation::getConfig();
    _backlightLevel = std::min(1024 * _config.backlight_level / 100, 1023);
}

void WeatherStationPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    _readConfig();
    _init();

    _weatherApi.setAPIKey(WSDraw::WSConfigType::getApiKey());
    _weatherApi.setQuery(WSDraw::WSConfigType::getApiQuery());

    _setScreen(ScreenType::MAIN); // TODO add config options for initial screen
    _initScreen();

    _fadeBacklight(0, _backlightLevel);

    // show progress bar during firmware updates
    int progressValue = -1;
    WebServer::Plugin::getInstance().setUpdateFirmwareCallback([this, progressValue](size_t position, size_t size) mutable {
        int progress = position * 100 / size;
        if (progressValue != progress) {
            if (progressValue == -1) {
                _setBacklightLevel(1023);
                _setScreen(ScreenType::TEXT_UPDATE);
            }
            setText(PrintString(F("Updating\n%d%%"), progress), FONTS_DEFAULT_MEDIUM);
            progressValue = progress;
        }
    });

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        if (!_touchpad.begin(MPR121_I2CADDR_DEFAULT, IOT_WEATHER_STATION_MPR121_PIN, &config.initTwoWire())) {
            Logger_error(F("Failed to initialize touch sensor"));
        }
        else {
            _touchpad.getMPR121().setThresholds(_config.touch_threshold, _config.released_threshold);
        }
    #endif

    #if IOT_ALARM_PLUGIN_ENABLED
        AlarmPlugin::setCallback(alarmCallback);
        #if IOT_WEATHER_STATION_HAS_TOUCHPAD
            // needs to be first callback to stop the event to be passed to other callbacks if the alarm is active
            _touchpad.addCallback(Mpr121Touchpad::EventType::RELEASED, 1, [this](const Mpr121Touchpad::Event &) {
                return _resetAlarm();
            });
        #endif
    #endif

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        _touchpad.addCallback(Mpr121Touchpad::EventType::RELEASED, 2, [this](const Mpr121Touchpad::Event &event) {
            __LDBG_printf("event %u", event.getType());
            // if (event.isSwipeLeft() || event.isDoublePress()) {
            //     _debug_println(F("left"));
            //     _setScreen((_currentScreen + NUM_SCREENS - 1) % NUM_SCREENS);
            // }
            // else if (event.isSwipeRight() || event.isPress()) {
            //     _debug_println(F("right"));
            //     _setScreen((_currentScreen + 1) % NUM_SCREENS);
            // }
            // else {
            //     return false;
            // }
            this->_setScreen(_getNextScreen(_getCurrentScreen()));
            return true;
        });
    #endif

    LoopFunctions::add(loop);

    #if IOT_WEATHER_STATION_WS2812_NUM
        WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType event, void *payload) {
            _fadeStatusLED();
        }, this);

        if (WiFi.isConnected()) {
            _fadeStatusLED();
        }
    #endif

    // SerialHandler::getInstance().addHandler(serialHandler, SerialHandler::RECEIVE);

    _installWebhooks();

/*
    WsClient::addClientCallback([this](WsClient::ClientCallbackTypeEnum_t type, WsClient *client) {
        if (type == WsClient::ClientCallbackTypeEnum_t::AUTHENTICATED) {
            // draw sends the screen capture to the web socket, do it for each new client
            _Scheduler.add(100, false, [this](Event::CallbackTimerPtr timer) {
                _redraw();
            });
        }
    });
    */
}

void WeatherStationPlugin::reconfigure(const String &source)
{
    if (source.equals(FSPGM(http))) {
        _installWebhooks();
    }
    else {
        auto oldLevel = _backlightLevel;
        _readConfig();
        #if IOT_WEATHER_STATION_HAS_TOUCHPAD
            _touchpad.getMPR121().setThresholds(_config.touch_threshold, _config.released_threshold);
        #endif
        _fadeBacklight(oldLevel, _backlightLevel);
        _setScreen(_getNextScreen(kNumScreens - 1));
    }
}

void WeatherStationPlugin::shutdown()
{
    #if IOT_ALARM_PLUGIN_ENABLED
        _resetAlarm();
        AlarmPlugin::setCallback(nullptr);
    #endif
    #if IOT_WEATHER_STATION_WS2812_NUM
        _pixelTimer.remove();
    #endif
    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        _touchpad.end();
    #endif
    _fadeTimer.remove();
    lock();
    delete _canvas;
    _canvas = nullptr;
    LoopFunctions::remove(loop);
    // SerialHandler::getInstance().removeHandler(serialHandler);

    _setBacklightLevel(1023);
    drawText(F("Rebooting\nDevice"), FONTS_DEFAULT_BIG, COLORS_DEFAULT_TEXT, true);
}

void WeatherStationPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%ux%u TFT " IOT_WEATHER_STATION_DRIVER), TFT_WIDTH, TFT_HEIGHT);
    #if IOT_WEATHER_STATION_WS2812_NUM
        output.printf_P(PSTR(HTML_S(br) "%ux WS2812 RGB LED"), IOT_WEATHER_STATION_WS2812_NUM);
    #endif
    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        output.printf_P(PSTR(HTML_S(br) "MPR121 Touch Pad"));
    #endif
}

void WeatherStationPlugin::_init()
{
    _setBacklightLevel(0);

    #if ILI9341_DRIVER
        __LDBG_printf("cs=%d dc=%d rst=%d spi=%u", TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST, SPI_FREQUENCY);
        _tft.begin(SPI_FREQUENCY);
    #else
        __LDBG_printf("cs=%d dc=%d rst=%d", TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);
        _tft.initR(INITR_BLACKTAB);
    #endif
    _tft.fillScreen(0);
    _tft.setRotation(0);
}

void WeatherStationPlugin::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Group(F("Weather Station"), false));
    webUI.addRow(WebUINS::Slider(F("bl_brightness"), F("Backlight Brightness"), false).append(WebUINS::NamedInt32(J(range_max), 1023)));
    if (_config.show_webui) {
        webUI.addRow(WebUINS::Screen(FSPGM(weather_station_webui_id, "ws_tft"), _canvas->width(), _canvas->height()));
    }
}

// void WeatherStationPlugin::serialHandler(uint8_t type, const uint8_t *buffer, size_t len)
// {
//     plugin._serialHandler(buffer, len);
// }

void WeatherStationPlugin::getValues(WebUINS::Events &array)
{
    __DBG_printf("show tft=%u", _config.show_webui);

    array.append(WebUINS::Values(F("bl_brightness"), _backlightLevel, true));

    if (_config.show_webui) {
        __DBG_printf("adding callOnce this=%p", this);
        // broadcast entire screen for each new client that connects
        LoopFunctions::callOnce([this]() {
            canvasUpdatedEvent(0, 0, TFT_WIDTH, TFT_HEIGHT);
        });
    }
}

void WeatherStationPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s value=%s has=%u", __S(id), __S(value), hasValue);
    if (hasValue && id.equals(F("bl_brightness"))) {
        int level =  value.toInt();
        _fadeBacklight(_backlightLevel, level);
        _backlightLevel = level;
    }
}

void WeatherStationPlugin::_drawEnvironmentalSensor(GFXCanvasCompressed& canvas, int16_t _offsetY)
{
    // __LDBG_printf("WSDraw::_drawEnvironmentalSensor()");

    auto sensor = SensorPlugin::getSensor<Sensor_BME280>(MQTT::SensorType::BME280);
    if (!sensor) {
        return;
    }
    auto values = sensor->readSensor();

    canvas.setFont(FONTS_WEATHER_DESCR);
    canvas.setTextColor(COLORS_WEATHER_DESCR);
    PrintString str(F("\n\nTemperature %.2f°C\nHumidity %.2f%%\nPressure %.2fhPa"), values.temperature, values.humidity, values.pressure);
    canvas.drawTextAligned(0, _offsetY, str, AdafruitGFXExtension::LEFT);
}

WeatherStationPlugin::IndoorValues WeatherStationPlugin::_getIndoorValues()
{
    auto sensor = SensorPlugin::getSensor<Sensor_BME280>(MQTT::SensorType::BME280);
    if (!sensor) {
        return IndoorValues(0, 0, 0);
    }
    auto values = sensor->readSensor();
    return IndoorValues(values.temperature, values.humidity, values.pressure);
}

void WeatherStationPlugin::_fadeBacklight(uint16_t fromLevel, uint16_t toLevel, uint8_t step)
{
    _setBacklightLevel(fromLevel);
    int8_t direction = fromLevel > toLevel ? -step : step;

    //TODO sometimes fading does not work...
    //probably when called multiple times before its done

    _Timer(_fadeTimer).add(Event::milliseconds(30), true, [fromLevel, toLevel, step, direction, this](Event::CallbackTimerPtr timer) mutable {

        if (abs(toLevel - fromLevel) > step) {
            fromLevel += direction;
        }
        else {
            fromLevel = toLevel;
            timer->disarm();
        }
        _setBacklightLevel(fromLevel);

    }, Event::PriorityType::TIMER);
}

void WeatherStationPlugin::_fadeStatusLED()
{
    #if IOT_WEATHER_STATION_WS2812_NUM
        __LDBG_printf("pin=%u", IOT_WEATHER_STATION_WS2812_PIN);

        int32_t color = 0x001500;
        int16_t dir = 0x100;

        NeoPixel_fillColor(_pixels, sizeof(_pixels), color);
        NeoPixel_espShow(IOT_WEATHER_STATION_WS2812_PIN, _pixels, sizeof(_pixels), true);

        _Timer(_pixelTimer).add(Event::milliseconds(50), true, [this, color, dir](::Event::CallbackTimerPtr timer) mutable {
            color += dir;
            if (color >= 0x003000) {
                dir = -dir;
                color = 0x003000;
            }
            else if (color <= 0) {
                color = 0;
                timer->disarm();
            }

            NeoPixel_fillColor(_pixels, sizeof(_pixels), color);
            NeoPixel_espShow(IOT_WEATHER_STATION_WS2812_PIN, _pixels, sizeof(_pixels), true);

        }, ::Event::PriorityType::TIMER);

    #endif
}

void WeatherStationPlugin::canvasUpdatedEvent(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (!_config.show_webui) {
        return;
    }
    if (_lockCanvasUpdateEvents && millis() < _lockCanvasUpdateEvents) {
        return;
    }
    __LDBG_S_IF(
        // debug
        if (_lockCanvasUpdateEvents) {
            __DBG_printf("queue lock removed");
            _lockCanvasUpdateEvents = 0;
        },
        // no debug
        _lockCanvasUpdateEvents = 0;
    )

    auto webSocketUI = WebUISocket::getServerSocket();
    // __DBG_printf("x=%d y=%d w=%d h=%d ws=%p empty=%u", x, y, w, h, webSocketUI, webSocketUI->getClients().isEmpty());
    if (webSocketUI && !isLocked() && !webSocketUI->getClients().isEmpty()) {
        Buffer buffer;

        WsClient::BinaryPacketType packetIdentifier = WsClient::BinaryPacketType::RGB565_RLE_COMPRESSED_BITMAP;
        buffer.write(reinterpret_cast<uint8_t *>(&packetIdentifier), sizeof(packetIdentifier));

        size_t len = strlen_P(SPGM(weather_station_webui_id));
        buffer.write(len);
        buffer.write_P(SPGM(weather_station_webui_id), len);
        if (buffer.length() & 0x01) { // the next part needs to be word aligned
            buffer.write(0);
        }

        // takes 42ms for 128x160 using GFXCanvasCompressedPalette
        // auto start = micros();

        if (lock()) {
            GFXCanvasRLEStream stream(*getCanvas(), x, y, w, h);
            char buf[128];
            size_t read;
            while((read = stream.readBytes(buf, sizeof(buf))) != 0) {
                buffer.write(buf, read);
            }
            unlock();
        }
        else {
            __DBG_printf("could not lock canvas");
            return;
        }

        // auto dur = micros() - start;
        // __DBG_printf("dur %u us", dur);
        // if (!canvas) {
        //     __DBG_printf("canvas was removed during update");
        //     return;
        // }

        uint8_t *ptr;
        buffer.write(0); // terminate with NUL byte
        len = buffer.length() - 1;
        buffer.move(&ptr);

        auto wsBuffer = webSocketUI->makeBuffer(ptr, len, false);
        // __LDBG_printf("buf=%p len=%u", wsBuffer, buffer.length());
        if (wsBuffer) {

            wsBuffer->lock();
            for(auto socket: webSocketUI->getClients()) {
                if (!socket->canSend()) { // queue full
                    _lockCanvasUpdateEvents = millis() + 5000;
                    __LDBG_printf("queue lock added");
                    break;
                }
                if (socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
                    socket->client()->setRxTimeout(10); // lower timeout
                    socket->binary(wsBuffer);
                }
            }
            wsBuffer->unlock();
            webSocketUI->_cleanBuffers();
        }
    }
}

#if IOT_ALARM_PLUGIN_ENABLED

void WeatherStationPlugin::alarmCallback(ModeType mode, uint16_t maxDuration)
{
    _getInstance()._alarmCallback(mode, maxDuration);
}

void WeatherStationPlugin::_alarmCallback(ModeType mode, uint16_t maxDuration)
{
    if (maxDuration == STOP_ALARM) {
        _resetAlarm();
        return;
    }
    if (!_resetAlarmFunc) {
        _resetAlarmFunc = [this](Event::CallbackTimerPtr timer) {
            #if IOT_WEATHER_STATION_WS2812_NUM
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
            #endif
            _alarmTimer.remove(); // make sure the scheduler is not calling a dangling pointer.. not using the TimerPtr in case it is not called from the scheduler
            _resetAlarmFunc = nullptr;
        };
    }

    // check if an alarm is already active
    if (!_alarmTimer) {
        #if IOT_WEATHER_STATION_WS2812_NUM
            BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::BlinkType::FAST, 0xff0000);
        #endif
    }

    if (maxDuration == 0) {
        maxDuration = 300; // limit time
    }
    // reset time if alarms overlap
    __LDBG_printf("alarm duration %u", maxDuration);
    _Timer(_alarmTimer).add(Event::seconds(maxDuration), false, _resetAlarmFunc);
}

bool WeatherStationPlugin::_resetAlarm()
{
    __LDBG_printf("reset=%u state=%u", _resetAlarmFunc ? 1 : 0, AlarmPlugin::getAlarmState());
    if (_resetAlarmFunc) {
        _resetAlarmFunc(*_alarmTimer);
        AlarmPlugin::resetAlarm();
        return true;
    }
    return false;
}

#endif

extern "C" void WeatherStationPlugin_unlock();

void WeatherStationPlugin_unlock()
{
    WeatherStationPlugin::_getInstance().unlock();
}

#endif
