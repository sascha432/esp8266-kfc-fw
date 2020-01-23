/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_WEATHER_STATION

#include "weather_station.h"
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <EventTimer.h>
#include <MicrosTimer.h>
#include <HeapStream.h>
#include <ProgmemStream.h>
#include <serial_handler.h>
#include <WebUISocket.h>
#include <AsyncBitmapStreamResponse.h>
#include <GFXCanvasRLEStream.h>
#include "web_server.h"
#include "progmem_data.h"
#include "kfc_fw_config.h"

// web accesss for screen capture
//
// screen capture
// http://192.168.0.56/images/screen_capture.bmp
//
//

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(weather_station_webui_id, "weather_station");

static WeatherStationPlugin plugin;

WeatherStationPlugin::WeatherStationPlugin() :
    WSDraw(),
    _updateTimer(0),
    _backlightLevel(1023),
    _pollInterval(0),
    _pollTimer(0),
    _httpClient(nullptr)
#if IOT_WEATHER_STATION_WS2812_NUM
    , _pixel(IOT_WEATHER_STATION_WS2812_NUM, IOT_WEATHER_STATION_WS2812_PIN)
#endif
{
#if DEBUG_IOT_WEATHER_STATION
    _debugDisplayCanvasBorder = false;
#endif
    pinMode(TFT_PIN_LED, OUTPUT);
    digitalWrite(TFT_PIN_LED, LOW);

    REGISTER_PLUGIN(this, "WeatherStationPlugin");
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    _touchpadDebug = false;
#endif
}

PGM_P WeatherStationPlugin::getName() const
{
    return PSTR("weather_station");
}

void WeatherStationPlugin::_sendScreenCaptureBMP(AsyncWebServerRequest *request)
{
    // _debug_printf_P(PSTR("WeatherStationPlugin::_sendScreenCapture(): is_authenticated=%u\n"), web_server_is_authenticated(request));

    if (web_server_is_authenticated(request)) {
        //auto response = new AsyncClonedBitmapStreamResponse(plugin.getCanvas().clone());
        debug_printf("AsyncBitmapStreamResponse\n");
        auto response = new AsyncBitmapStreamResponse(plugin.getCanvas());
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache();
        httpHeaders.setWebServerResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void WeatherStationPlugin::_installWebhooks()
{
    _debug_printf_P(PSTR("WeatherStationPlugin::_installWebhooks(): Installing web handler for screen capture, server=%p\n"), get_web_server_object());
    web_server_add_handler(F("/images/screen_capture.bmp"), _sendScreenCaptureBMP);
}

void WeatherStationPlugin::setup(PluginSetupMode_t mode) {

    auto cfg = config._H_GET(Config().weather_station.config);
    _isMetric = cfg.is_metric;
    _timeFormat24h = cfg.time_format_24h;
    _pollInterval = cfg.weather_poll_interval * 60000UL;
    _backlightLevel = std::min(1024 * cfg.backlight_level / 100, 1023);

    _tft.initR(INITR_BLACKTAB);
    _tft.fillScreen(0);
    _tft.setRotation(0);

    _weatherApi.setAPIKey(config._H_STR(Config().weather_station.openweather_api_key));
    _weatherApi.setQuery(config._H_STR(Config().weather_station.openweather_api_query));
    _getWeatherInfo([this](bool status) {
        _draw();
    });

    _draw();
    _fadeBacklight(0, _backlightLevel);

#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (!_touchpad.begin(MPR121_I2CADDR_DEFAULT, IOT_WEATHER_STATION_MPR121_PIN, &config.initTwoWire())) {
        Logger_error(F("Failed to initialize touch sensor"));
    }
#endif

#if IOT_WEATHER_STATION_WS2812_NUM
    uint8_t color = 128;
    _pixel.begin();
    _pixelTimer.add(25, true, [this, color](EventScheduler::TimerPtr timer) mutable {
        color--;
        for(uint8_t n = 0; n < IOT_WEATHER_STATION_WS2812_NUM; n++) {
            _pixel.setPixelColor(n, color);
        }
        _pixel.show();
        if (color == 0) {
            timer->detach();
        }
    });
#endif

    LoopFunctions::add(loop);
    SerialHandler::getInstance().addHandler(serialHandler, SerialHandler::RECEIVE);

    _installWebhooks();

/*
    WsClient::addClientCallback([this](WsClient::ClientCallbackTypeEnum_t type, WsClient *client) {
        if (type == WsClient::ClientCallbackTypeEnum_t::AUTHENTICATED) {
            // draw sends the screen capture to the web socket, do it for each new client
            Scheduler.addTimer(100, false, [this](EventScheduler::TimerPtr timer) {
                _draw();
            });
        }
    });
    */
}

void WeatherStationPlugin::reconfigure(PGM_P source)
{
    _installWebhooks();
}

void WeatherStationPlugin::restart()
{
#if IOT_WEATHER_STATION_WS2812_NUM
    _pixelTimer.remove();
#endif
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    _touchpad.end();
#endif
    _fadeTimer.remove();
    analogWrite(TFT_PIN_LED, 0);
    LoopFunctions::remove(loop);
    SerialHandler::getInstance().removeHandler(serialHandler);
    if (_httpClient) {
        _httpClient->abort();
    }
}

bool WeatherStationPlugin::hasReconfigureDependecy(PluginComponent *plugin) const
{
    return plugin->nameEquals(FSPGM(http));
}

bool WeatherStationPlugin::hasStatus() const
{
    return true;
}

void WeatherStationPlugin::getStatus(Print &output)
{
    _debug_printf_P(PSTR("WeatherStationPlugin::getStatus()\n"));
    output.printf_P(PSTR("Weather Station Plugin"));
}

void WeatherStationPlugin::loop()
{
    plugin._loop();
}

bool WeatherStationPlugin::hasWebUI() const
{
    return true;
}

WebUIInterface *WeatherStationPlugin::getWebUIInterface()
{
    return this;
}

void WeatherStationPlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Weather Station"), false);

    row = &webUI.addRow();
    row->addScreen(FSPGM(weather_station_webui_id), _canvas.width(), _canvas.height());
}


void WeatherStationPlugin::serialHandler(uint8_t type, const uint8_t *buffer, size_t len)
{
    plugin._serialHandler(buffer, len);
}

void WeatherStationPlugin::getValues(JsonArray &array) {
    _debug_printf_P(PSTR("WeatherStationPlugin::getValues()\n"));

    // redraw screen in main loop
    Scheduler.addTimer(1, false, [this](EventScheduler::TimerPtr timer) {
        _draw();
    });
}

void WeatherStationPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    _debug_printf_P(PSTR("WeatherStationPlugin::setValue()\n"));
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSSET, "WSSET", "<on/off>,<touchpad/timeformat24h/metrics>", "Enable/disable function");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSLED, "WSLED", "<r>,<g>,<b>", "Set LED color");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSBL, "WSBL", "<level=0-1023>", "Set backlight level");

void WeatherStationPlugin::atModeHelpGenerator()
{
    auto name = getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WSSET), name);
#if IOT_WEATHER_STATION_WS2812_NUM
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WSLED), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WSBL), name);
}

bool WeatherStationPlugin::atModeHandler(AtModeArgs &args)
{
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSSET))) {
        if (args.requireArgs(2, 2)) {
            bool state = args.isTrue(0);
            if (args.equalsIgnoreCase(1, F("touchpad"))) {
                _touchpadDebug = state;
                args.printf_P(PSTR("touchpad debug=%u"), state);
            }
            else if (args.equalsIgnoreCase(1, F("timeformat24h"))) {
                _timeFormat24h = state;
                _draw();
                args.printf_P(PSTR("time format 24h=%u"), state);
            }
            else if (args.equalsIgnoreCase(1, F("metrics"))) {
                _isMetric = state;
                _draw();
                args.printf_P(PSTR("metrics=%u"), state);
            }
            else {
                args.print("Invalid type");
            }
        }
        return true;
    }
#endif
#if IOT_WEATHER_STATION_WS2812_NUM
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSLED))) {
        if (args.requireArgs(3, 3)) {
            uint8_t r = (uint8_t)args.toNumber(0);
            uint8_t g = (uint8_t)args.toNumber(1);
            uint8_t b = (uint8_t)args.toNumber(2);
            uint32_t color = r | (g << 8) | (b << 16);
            for(uint n = 0; n < IOT_WEATHER_STATION_WS2812_NUM; n++) {
                _pixel.setPixelColor(n, color);
            }
            _pixel.show();
            args.printf_P(PSTR("led=%02x%02x%02x"), r, g, b);
        }
        return true;
    }
#endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSBL))) {
        if (args.requireArgs(1, 1)) {
            uint16_t level = args.toIntMinMax<uint16_t>(0, 0, 1023);
            _fadeBacklight(_backlightLevel, level);
            _backlightLevel = level;
            args.printf_P(PSTR("backlight=%d"), level);
        }
        return true;
    }
}

#endif

void WeatherStationPlugin::_httpRequest(const String &url, uint16_t timeout, JsonBaseReader *jsonReader, Callback_t finishedCallback)
{
	if (_httpClient != nullptr) {
        debug_println(F("WeatherStationPlugin::_httpRequest() _httpClient not null, cannot create request"));
        return;
	}

	_httpClient = new asyncHTTPrequest();

    auto onData = [jsonReader](void *ptr, asyncHTTPrequest *request, size_t available) {

        _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(): onData() available=%u\n"), available);

        uint8_t buffer[64];
        size_t len;
        HeapStream stream(buffer);
        jsonReader->setStream(&stream);
        while((len = request->responseRead(buffer, sizeof(buffer))) > 0) {
            stream.setLength(len);
            if (!jsonReader->parseStream()) {
                _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(): onRead(): parseStream() = false\n"));
                request->abort();
                request->onData(nullptr);
                break;
            }
        }
    };

    _httpClient->onData(onData);

	_httpClient->onReadyStateChange([this, url, finishedCallback, jsonReader, onData](void *, asyncHTTPrequest *request, int readyState) {
        _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(): onReadyStateChange() readyState=%u\n"), readyState);
		if (readyState == 4) {
            bool status;
			int httpCode;

             request->onData(nullptr); // disable callback, if any data is left, onPoll() will call it again
			if ((httpCode = request->responseHTTPcode()) == 200) {

                // read rest of the data that was not processed by onData()
                if (request->available()) {
                    onData(nullptr, request, request->available());
                }

                _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(), url=%s, response OK, available = %u\n"), url.c_str(), request->available());
                status = true;

			} else {

                String message;
                int code = 0;

                // read response
                uint8_t buffer[64];
                HeapStream stream(buffer);

                JsonCallbackReader reader(stream, [&message, &code](const String &keyStr, const String &valueStr, size_t partialLength, JsonBaseReader &json) {
                    auto key = keyStr.c_str();
                    if (!strcmp_P(key, PSTR("cod"))) {
                        code = valueStr.toInt();
                    }
                    else if (!strcmp_P(key, PSTR("message"))) {
                        message = valueStr;
                    }
                    return true;
                });
                reader.initParser();

                size_t len;
                while((len = request->responseRead(buffer, sizeof(buffer))) > 0) {
                    stream.setLength(len);
                    if (!reader.parseStream()) {
                        break;
                    }
                }

                _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(), http code=%d, code=%d, message=%s, url=%s\n"), httpCode, code, message.c_str(), url.c_str());
                _weatherError = F("Failed to load data");
                status = false;

                // Logger_error(F("Weather Station: HTTP request failed with HTTP code = %d, API code = %d, message = %s, url = %s"), httpCode, code, message.c_str(), url.c_str());
			}

            // delete object with a delay and inside the main loop, otherwise the program crashes in random places
            auto tmp = _httpClient;
            _httpClient = nullptr;
            delete jsonReader;

            Scheduler.addTimer(100, false, [tmp, finishedCallback, status](EventScheduler::TimerPtr timer) {
                _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(), delete _httpClient + finishedCallback\n"));
                delete tmp;
                finishedCallback(status);
            });
		}
	});
	_httpClient->setTimeout(timeout);

    _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest() HTTP request=%s\n"), url.c_str());

	if (!_httpClient->open("GET", url.c_str()) || !_httpClient->send()) {
        delete _httpClient;
        delete jsonReader;
        _httpClient = nullptr;

        _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest() client error, url=%s\n"), url.c_str());
        _weatherError = F("Client error");
        finishedCallback(false);

        Logger_error(F("Weather Station: HTTP request failed, %s, url = %s"), _weatherError.c_str(), url.c_str());
    }
}


void WeatherStationPlugin::_getWeatherInfo(Callback_t finishedCallback)
{
#if DEBUG_IOT_WEATHER_STATION
    auto prev = finishedCallback;
    finishedCallback = [this, prev](bool status) {
        _weatherApi.dump(DebugSerial);
        prev(status);
    };

    if (_weatherError.indexOf("avail") != -1) { // load dummy data
        _weatherError = F("Dummy data");
        PGM_P data = PSTR("{\"coord\":{\"lon\":-123.07,\"lat\":49.32},\"weather\":[{\"id\":500,\"main\":\"Rain\",\"description\":\"light rain\",\"icon\":\"10n\"},{\"id\":701,\"main\":\"Mist\",\"description\":\"mist\",\"icon\":\"50n\"}],\"base\":\"stations\",\"main\":{\"temp\":277.55,\"pressure\":1021,\"humidity\":100,\"temp_min\":275.37,\"temp_max\":279.26},\"visibility\":8047,\"wind\":{\"speed\":1.19,\"deg\":165},\"rain\":{\"1h\":0.93},\"clouds\":{\"all\":90},\"dt\":1575357173,\"sys\":{\"type\":1,\"id\":5232,\"country\":\"CA\",\"sunrise\":1575301656,\"sunset\":1575332168},\"timezone\":-28800,\"id\":6090785,\"name\":\"North Vancouver\",\"cod\":200}");
        ProgmemStream stream(data, strlen(data));
        _weatherApi.clear();
        if (!_weatherApi.parseWeatherData(stream)) {
            _weatherError = F("Invalid data");
        }
        finishedCallback(true);
        return;
    }

#endif

    _httpRequest(_weatherApi.getWeatherApiUrl(), std::min(config._H_GET(Config().weather_station.config).api_timeout, (uint16_t)10), _weatherApi.getWeatherInfoParser(), finishedCallback);
}

void WeatherStationPlugin::_getWeatherForecast(Callback_t finishedCallback)
{
#if DEBUG_IOT_WEATHER_STATION
    auto prev = finishedCallback;
    finishedCallback = [this, prev](bool status) {
        _weatherApi.dump(DebugSerial);
        prev(status);
    };
#endif

    _httpRequest(_weatherApi.getForecastApiUrl(), std::min(config._H_GET(Config().weather_station.config).api_timeout, (uint16_t)10), _weatherApi.getWeatherForecastParser(), finishedCallback);
}

void WeatherStationPlugin::_fadeBacklight(uint16_t fromLevel, uint16_t toLevel, int8_t step)
{
    int8_t direction = fromLevel > toLevel ? -step : step;
    analogWrite(TFT_PIN_LED, fromLevel);

    if (fromLevel != toLevel) {
        _fadeTimer.add(10, true, [fromLevel, toLevel, direction, step](EventScheduler::TimerPtr timer) mutable {
            if (abs(toLevel - fromLevel) > step) {
                fromLevel += direction;
            } else {
                fromLevel = toLevel;
                timer->detach();
            }
            analogWrite(TFT_PIN_LED, fromLevel);
        }, EventScheduler::PRIO_HIGH);
    }
}

void WeatherStationPlugin::_loop()
{
    time_t _time = time(nullptr);

#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    uint8_t x = 0, y = 0;
    auto pending = _touchpad.isEventPending();
    _touchpad.get(x, y);
    if (pending) {
        debug_printf_P(PSTR("touchpad: x=%u y=%u\n"), x, y);
    }

    if (_touchpadDebug) {
        SpeedBooster speedBooster;
        uint16_t col1 = ST77XX_RED;
        uint16_t col2 = ST77XX_WHITE;
        _canvas.fillScreen(COLORS_BACKGROUND);
        for(int yy = 1; yy <= 7; yy++) {
            _canvas.drawLine(10, yy * 10, 9 * 13, yy * 10, yy == y ? col1 : col2);
        }
        for(int xx = 1; xx <= 13; xx++) {
            _canvas.drawLine(xx * 9, 10, xx * 9, 70, xx == x ? col1 : col2);
        }

        _displayScreen(0, 0, TFT_WIDTH, 80);
        return;
    }
#endif

    if (_pollInterval && is_millis_diff_greater(_pollTimer, _pollInterval)) {
        _pollTimer = millis();
        _debug_println(F("WeatherStationPlugin::_loop(): poll interval"));
        //TODO

        _getWeatherInfo([this](bool status) {
            _draw();
        });

    }
    else if (is_millis_diff_greater(_updateTimer, 60000)) { // update once per minute
        _draw();
        _updateTimer = millis();
    }
    else if (_currentScreen == 0 && _lastTime != _time) {
        if (_time - _lastTime > 10) {   // time jumped, redraw everything
            _draw();
        }
        else {
            _updateTime();
        }
    }
}

void WeatherStationPlugin::_serialHandler(const uint8_t *buffer, size_t len)
{
    const uint8_t numScreens = 1;
    auto ptr = buffer;
    while(len--) {
        switch(*ptr++) {
#if 0
            case '+': // next screen
                _currentScreen++;
                _currentScreen %= numScreens;
                _draw();
                break;
            case '-': // prev screen
                _currentScreen += numScreens;
                _currentScreen--;
                _currentScreen %= numScreens;
                _draw();
                break;
            case 'd': // redraw
                _draw();
                break;
            case 'u': // update weather info
                _getWeatherInfo([this](bool status) {
                    _draw();
                });
                break;
            case 'f': // update weather info
                _getWeatherForecast([this](bool status) {
                    _draw();
                });
                break;
            case 'g': // gfx debug
                _debugDisplayCanvasBorder = !_debugDisplayCanvasBorder;
                _draw();
                break;
            case 's':
                if (!_isScrolling()) {
                    _doScroll();
                    Scheduler.addTimer(20, true, [this](EventScheduler::TimerPtr timer) {
                        _scrollTimer(timer);
                    });
                }
                break;
#endif
        }
    }
}


// void WeatherStationPlugin::_drawRGBBitmap(int16_t x, int16_t y, uint16_t *pcolors, int16_t w, int16_t h) {
// #if DEBUG_IOT_WEATHER_STATION

//     static int16_t max_height = 0;
//     max_height = std::max(max_height, h);

//     // _tft.drawRGBBitmap(x, y, pcolors, w, h);
//     _broadcastRGBBitmap(x, y, pcolors, w, h);

//     if (_debugDisplayCanvasBorder) {
//         _debug_printf_P(PSTR("WeatherStationPlugin::_drawRGBBitmap(): x=%d, y=%d, w=%u, h=%u (max. %u)\n"), x, y, w, h, max_height);
//         uint16_t colors[] = { ST77XX_CYAN, ST77XX_RED, ST77XX_YELLOW, ST77XX_BLUE };
//         _tft.drawRect(x, y, w, h, colors[y % 4]);
//     }

// #else

//     _tft.drawRGBBitmap(x, y, pcolors, w, h);
//     _broadcastRGBBitmap(x, y, pcolors, w, h);

// #endif
// }

void WeatherStationPlugin::_broadcastCanvas(int16_t x, int16_t y, int16_t w, int16_t h)
{
    auto webSocketUI = WsWebUISocket::getWsWebUI();
    if (webSocketUI && !webSocketUI->getClients().isEmpty()) {
        Buffer buffer;

        WebSocketBinaryPacketUnqiueId_t packetIdentifier = RGB565_RLE_COMPRESSED_BITMAP;
        buffer.write(reinterpret_cast<uint8_t *>(&packetIdentifier), sizeof(packetIdentifier));

        const uint8_t len = constexpr_strlen_P(SPGM(weather_station_webui_id));
        buffer.write(len);
        buffer.write_P(SPGM(weather_station_webui_id), len);

        GFXCanvasRLEStream stream(getCanvas(), x, y, w, h);
        while(stream.available()) {
            buffer.write(stream.read());
        }

        auto wsBuffer = webSocketUI->makeBuffer(buffer.get(), buffer.length());
        buffer.clear();
        if (wsBuffer) {

            wsBuffer->lock();
            for(auto socket: webSocketUI->getClients()) {
                if (socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
                    socket->binary(wsBuffer);
                }
            }
            wsBuffer->unlock();
            webSocketUI->_cleanBuffers();
        }
    }

}

#endif
