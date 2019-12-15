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
// display queue status
// http://192.168.0.56/invoke_screen_capture?query
//
// create screen capture
// http://192.168.0.56/invoke_screen_capture
//
//

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


static WeatherStationPlugin plugin;

WeatherStationPlugin::WeatherStationPlugin() :
    WSDraw(),
    _updateTimer(0),
    _backlightLevel(1023),
    _pollInterval(0),
    _pollTimer(0),
    _httpClient(nullptr)
{
#if DEBUG_IOT_WEATHER_STATION
    _debugDisplayCanvasBorder = false;
#endif
    pinMode(TFT_PIN_LED, OUTPUT);
    digitalWrite(TFT_PIN_LED, LOW);

    register_plugin(this);
}

PGM_P WeatherStationPlugin::getName() const {
    return PSTR("weather_station");
}

void WeatherStationPlugin::_sendScreenCaptureBMP(AsyncWebServerRequest *request) {
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

void WeatherStationPlugin::_installWebhooks() {

    auto server = get_web_server_object();
    if (server) {
        _debug_printf_P(PSTR("WeatherStationPlugin::_installWebhooks(): Installing web handler for screen capture\n"));
        server->on("/images/screen_capture.bmp", _sendScreenCaptureBMP);
    }
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

    LoopFunctions::add(loop);
    SerialHandler::getInstance().addHandler(serialHandler, SerialHandler::RECEIVE);

    _installWebhooks();

    WsClient::addClientCallback([this](WsClient::ClientCallbackTypeEnum_t type, WsClient *client) {
        if (type == WsClient::ClientCallbackTypeEnum_t::AUTHENTICATED) {
            // draw sends the screen capture to the web socket, do it for each new client
            Scheduler.addTimer(100, false, [this](EventScheduler::TimerPtr timer) {
                _draw();
            });

        }
    });
}

void WeatherStationPlugin::reconfigure(PGM_P source) {

    _installWebhooks();
}

bool WeatherStationPlugin::hasReconfigureDependecy(PluginComponent *plugin) const {
    return plugin->nameEquals(F("http"));
}

bool WeatherStationPlugin::hasStatus() const {
    return true;
}

const String WeatherStationPlugin::getStatus() {
    _debug_printf_P(PSTR("WeatherStationPlugin::getStatus()\n"));
    PrintHtmlEntitiesString str;
    str.printf_P(PSTR("WeatherStationPlugin"));
    return str;
}

void WeatherStationPlugin::loop() {
    plugin._loop();
}

void WeatherStationPlugin::serialHandler(uint8_t type, const uint8_t *buffer, size_t len) {
    plugin._serialHandler(buffer, len);
}

void WeatherStationPlugin::_httpRequest(const String &url, uint16_t timeout, JsonBaseReader *jsonReader, Callback_t finishedCallback) {

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


void WeatherStationPlugin::_getWeatherInfo(Callback_t finishedCallback) {

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

void WeatherStationPlugin::_getWeatherForecast(Callback_t finishedCallback) {

#if DEBUG_IOT_WEATHER_STATION
    auto prev = finishedCallback;
    finishedCallback = [this, prev](bool status) {
        _weatherApi.dump(DebugSerial);
        prev(status);
    };
#endif

    _httpRequest(_weatherApi.getForecastApiUrl(), std::min(config._H_GET(Config().weather_station.config).api_timeout, (uint16_t)10), _weatherApi.getWeatherForecastParser(), finishedCallback);
}

void WeatherStationPlugin::_fadeBacklight(uint16_t fromLevel, uint16_t toLevel, int8_t step) {
    int8_t direction = fromLevel > toLevel ? -step : step;
    analogWrite(TFT_PIN_LED, fromLevel);

    if (fromLevel != toLevel) {
        Scheduler.addTimer(10, true, [fromLevel, toLevel, direction, step](EventScheduler::TimerPtr timer) mutable {
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

void WeatherStationPlugin::_loop() {
    time_t _time = time(nullptr);

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

void WeatherStationPlugin::_serialHandler(const uint8_t *buffer, size_t len) {
    const uint8_t numScreens = 1;
    auto ptr = buffer;
    while(len--) {
        switch(*ptr++) {
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
            case 't': // switch 12/24h time format
                _timeFormat24h = !_timeFormat24h;
                _draw();
                break;
            case 'm': // switch metric/imperial
                _isMetric = !_isMetric;
                _draw();
                break;
            case '0': // turn backlight off
                _fadeBacklight(_backlightLevel, 0);
                break;
            case '1': // turn backlight on
                _fadeBacklight(0, _backlightLevel);
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
            case 'p': // pause
                LoopFunctions::remove(loop);
                break;
            case 'r': // resume
                LoopFunctions::add(loop);
                break;
            case 'g': // gfx debug
                _debugDisplayCanvasBorder = !_debugDisplayCanvasBorder;
                _draw();
                break;
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

void WeatherStationPlugin::_broadcastCanvas(int16_t x, int16_t y, int16_t w, int16_t h) {

    auto webSocketUI = WsWebUISocket::getWsWebUI();
    if (webSocketUI && !webSocketUI->getClients().isEmpty()) {
        Buffer buffer;

        WebSocketBinaryPacketUnqiueId_t packetIdentifier = RGB565_RLE_COMPRESSED_BITMAP;
        buffer.write(reinterpret_cast<uint8_t *>(&packetIdentifier), sizeof(packetIdentifier));

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
