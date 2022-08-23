/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "weather_station.h"
#include "WeatherStationBase.h"
#include "Logger.h"
#include "WSDraw.h"

#if DEBUG_IOT_WEATHER_STATION
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

WeatherStationBase::WeatherStationBase() :
    _pollDataLastMillis(0),
    _pollDataRetries(0),
    _backlightLevel(1023),
    _updateCounter(0),
    _toggleScreenTimer(0),
    _toggleScreenTimeout(0),
    _lockCanvasUpdateEvents(0)
{
    #if _MSC_VER || REAL_DEVICE == 0
        String stream = F("{\"coord\":{\"lon\":-123.07,\"lat\":49.32},\"weather\":[{\"id\":500,\"main\":\"Rain\",\"description\":\"light rain\",\"icon\":\"10n\"},{\"id\":701,\"main\":\"Mist\",\"description\":\"mist\",\"icon\":\"50n\"}],\"base\":\"stations\",\"main\":{\"temp\":277.55,\"pressure\":1021,\"humidity\":100,\"temp_min\":275.37,\"temp_max\":279.26},\"visibility\":8047,\"wind\":{\"speed\":1.19,\"deg\":165},\"rain\":{\"1h\":0.93},\"clouds\":{\"all\":90},\"dt\":1575357173,\"sys\":{\"type\":1,\"id\":5232,\"country\":\"CA\",\"sunrise\":1575301656,\"sunset\":1575332168},\"timezone\":-28800,\"id\":6090785,\"name\":\"North Vancouver\",\"cod\":200}");
        _weatherApi.clear();
        if (!_weatherApi.parseWeatherData(stream)) {
            _weatherApi.getWeatherInfo().setError(F("Invalid data"));
        }
    #endif
}

void WeatherStationBase::_pollDataTimerCallback(Event::CallbackTimerPtr timer)
{
    ws_plugin._getWeatherInfo([](int16_t code, KFCRestAPI::HttpRequest &request) {
        ws_plugin._openWeatherAPICallback(code, request);
    });
}

void WeatherStationBase::_openWeatherAPICallback(int16_t code, KFCRestAPI::HttpRequest &request)
{
    __LDBG_printf("code=%d message=%s url=%s", code, __S(request.getMessage()), __S(request.getUrl()));
    if (code != 200) {
        PrintString message(F("OpenWeatherAPI http status=%d error=%s wifi=%u url=%s"), code, request.getMessage().c_str(), WiFi.isConnected(), request.getUrl());
        Logger_error(message);
        _weatherApi.getWeatherInfo().setError(F("OpenWeatherAPI\nHTTP Error"));
        _pollDataUpdateLastTime(false);
    }
    else if (!_weatherApi.getWeatherInfo().hasData()) {
        PrintString message(F("OpenWeatherAPI data=invalid message=%s url=%s"), request.getMessage().c_str(), request.getUrl());
        Logger_error(message);
        _weatherApi.getWeatherInfo().setError(F("OpenWeatherAPI\nInvalid Data"));
        _pollDataUpdateLastTime(false);
    }
    else {
        _pollDataUpdateLastTime(true);
    }
    redraw();
}

void WeatherStationBase::_wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        __LDBG_printf("poll weather stopped");
        _Timer(_pollDataTimer).remove();
        redraw();
    }
    else if (event == WiFiCallbacks::EventType::CONNECTED) {
        auto next = 10000;
        __LDBG_printf("poll weather next=%u", next);
        _Timer(_pollDataTimer).add(next, false, _pollDataTimerCallback);
        #if IOT_WEATHER_STATION_WS2812_NUM
            _fadeStatusLED();
        #endif
        redraw();
    }
}

void WeatherStationBase::_pollDataUpdateLastTime(bool success)
{
    _pollDataLastMillis = millis();
    uint32_t next;
    if (success) {
        _pollDataRetries = 0;
        next = _config.getPollIntervalMillis();
    }
    else {
        _pollDataRetries++;
        next = _config.getPollIntervalMillis();
    }
    if (_pollDataRetries > kPollDataRetries) {
        next = 3600 * (kPollDataRetries - _pollDataRetries);
    }
    __LDBG_printf("success=%u retries=%u next=%u", success, _pollDataRetries, next);
    _Timer(_pollDataTimer).add(std::max(kMinPollDataInterval, next), false, _pollDataTimerCallback);
}

void WeatherStationBase::_httpRequest(const String &url, int timeout, JsonBaseReader *jsonReader, HttpRequestCallback callback)
{
    #if DEBUG_IOT_WEATHER_STATION && 0
        auto prev = callback;
        callback = [this, prev](int16_t httpCode, const String &error) {
            _weatherApi.dump(DEBUG_OUTPUT);
            prev(httpCode, error);
        };
    #endif
    __LDBG_printf("url=%s", url.c_str());
    auto rest = new ::WeatherStation::RestAPI(url);
    rest->call(jsonReader, std::max(15, timeout), callback);
}

void WeatherStationBase::_getWeatherInfo(HttpRequestCallback callback)
{
    _httpRequest(_weatherApi.getWeatherApiUrl(), _config.api_timeout, _weatherApi.getWeatherInfoParser(), callback);
}

void WeatherStationBase::_getWeatherForecast(HttpRequestCallback callback)
{
    _httpRequest(_weatherApi.getForecastApiUrl(), _config.api_timeout, _weatherApi.getWeatherForecastParser(), callback);
}

void WeatherStationBase::_draw()
{
    Base::_draw();
    _updateCounter = 0;
    MUTEX_LOCK_BLOCK(_lock) {
        if (_redrawFlag) {
            _redrawFlag = false;
        }
    }
}

void WeatherStationBase::_loop()
{
    if (isLocked()) {
        return;
    }

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        if (_touchpadDebug) {
            const auto &coords = _touchpad.get();
            uint16_t colorTouched = COLORS_RED;
            uint16_t colorPredict = COLORS_YELLOW;
            uint16_t colorGrid = COLORS_WHITE;
            _canvas->fillScreen(COLORS_BACKGROUND);
            for(int yy = 1; yy <= 8; yy++) {
                _canvas->drawLine(10, yy * 10, 8 * 14, yy * 10, yy == coords.y ? colorTouched : (yy == _touchpad._event._predict.y ? colorPredict : colorGrid));
            }
            for(int xx = 1; xx <= 13; xx++) {
                _canvas->drawLine(xx * 8, 10, xx * 8, 8 * 10, xx == coords.x ? colorTouched : (xx == _touchpad._event._predict.x ? colorPredict : colorGrid));
            }

            _displayScreen(0, 0, TFT_WIDTH, 85);
            return;
        }
    #endif

    MUTEX_LOCK_BLOCK(_lock) {
        if (_redrawFlag) {
            _draw();
            _redrawFlag = false;
            return;
        }
    }

    if (_currentScreen == ScreenType::TEXT_CLEAR || _currentScreen == ScreenType::TEXT_UPDATE) {
        _draw();
    }
    else {

        if (_toggleScreenTimeout) {
            uint32_t dur = millis() - _toggleScreenTimer;
            if (dur > _toggleScreenTimeout) {
                // __LDBG_printf("timeout=%u screen=%u next=%u", _toggleScreenTimeout, _getCurrentScreen(), _getNextScreen(_getCurrentScreen()));
                _setScreen(_getNextScreen(_getCurrentScreen()));
                _draw();
                return;
            }
        }

        time_t _time = time(nullptr);
        if (_currentScreen < ScreenType::NUM_SCREENS && _lastTime != _time) {
            _updateCounter++;
            if ((static_cast<uint32_t>(_time - _lastTime) > 10) || (_updateCounter % 60 == 0))  {
                // redraw the entire screen when the time changes too much or once per minute
                // this keeps the indoor data and the sun and moon info up to date
                _draw();
            }
            else {
                bool do5secUpdate = (_updateCounter % 5 == 0);
                if (_currentScreen == ScreenType::MAIN) {
                    if (do5secUpdate) {
                        // update main section every 5 seconds
                        _updateScreenMain();
                    }
                    else {
                        _updateIndoorClimateBottom();
                    }
                }
                else if (_currentScreen == ScreenType::MOON_PHASE) {
                    _updateMoonPhase();
                    return;
                }
                #if DEBUG_IOT_WEATHER_STATION
                    else if (_currentScreen == ScreenType::DEBUG_INFO) {
                        // update debug screen every second
                        _updateScreenDebug();
                        return;
                    }
                #endif
                else if (_currentScreen == ScreenType::WORLD_CLOCK) {
                    // update clock screen every second
                    _updateScreenWorldClock();
                    return;
                }
                else if (_currentScreen == ScreenType::INDOOR) {
                    _updateScreenIndoorClimate();
                }
                else if (do5secUpdate) {
                    // update forecast section every 5 seconds
                    if (_currentScreen == ScreenType::FORECAST) {
                        _updateScreenForecast();
                    }
                    else if (_currentScreen == ScreenType::INFO) {
                        _updateScreenInfo();
                    }
                }

                // update time every second
                _updateScreenTime();
            }
        }
        #if DEBUG_IOT_WEATHER_STATION
            else if (_currentScreen == ScreenType::DEBUG_INFO) {
                // update debug screen as often as possible
                _updateScreenDebug();
            }
        #endif

    }
}

void WeatherStationBase::_setScreen(ScreenType screen, int16_t timeout)
{
    // verify if screen is valid
    _currentScreen = _getScreen(screen, true);
    auto n = static_cast<uint8_t>(_currentScreen);

    // __LDBG_printf("screen=%u new_screen=%u timeout=%d", screen, _currentScreen, timeout);

    // reset timeout
    _toggleScreenTimer = millis();
    _toggleScreenTimeout = 0;

    if (_currentScreen < ScreenType::NUM_SCREENS) {
        if (timeout == -1) {
            // default timeout
            _toggleScreenTimeout = _config.screenTimer[n] * 1000U;
        }
        else {
            // custom timeout
            _toggleScreenTimeout = timeout * 1000U;
        }
    }

    // __LDBG_printf("screen=%u new_screen=%u timeout=%u", screen, newScreen, _toggleScreenTimeout);
    redraw();
}

WeatherStationBase &WeatherStationBase::_getInstance()
{
    return ws_plugin;
}
