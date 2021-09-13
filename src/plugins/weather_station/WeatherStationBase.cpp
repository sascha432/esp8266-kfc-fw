/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "WeatherStationBase.h"
#include "WSScreen.h"
#include "Logger.h"

#if _MSC_VER
#include <StreamString.h>
#endif

WeatherStationBase::WeatherStationBase() :
    _pollDataLastMillis(0),
    _pollDataRetries(0),
    _updateCounter(0),
    _backlightLevel(1023),
    _toggleScreenTimer(0),
    _lockCanvasUpdateEvents(0)
{
#if _MSC_VER
    StreamString stream = F("{\"coord\":{\"lon\":-123.07,\"lat\":49.32},\"weather\":[{\"id\":500,\"main\":\"Rain\",\"description\":\"light rain\",\"icon\":\"10n\"},{\"id\":701,\"main\":\"Mist\",\"description\":\"mist\",\"icon\":\"50n\"}],\"base\":\"stations\",\"main\":{\"temp\":277.55,\"pressure\":1021,\"humidity\":100,\"temp_min\":275.37,\"temp_max\":279.26},\"visibility\":8047,\"wind\":{\"speed\":1.19,\"deg\":165},\"rain\":{\"1h\":0.93},\"clouds\":{\"all\":90},\"dt\":1575357173,\"sys\":{\"type\":1,\"id\":5232,\"country\":\"CA\",\"sunrise\":1575301656,\"sunset\":1575332168},\"timezone\":-28800,\"id\":6090785,\"name\":\"North Vancouver\",\"cod\":200}");
    _weatherApi.clear();
    if (!_weatherApi.parseWeatherData(&stream)) {
        _weatherApi.getWeatherInfo().setError(F("Invalid data"));
    }
#endif
}

void WeatherStationBase::_wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        _Timer(_pollDataTimer).remove();
    }
    else if (event == WiFiCallbacks::EventType::CONNECTED) {
        //_Timer(_pollDataTimer).add(_pollDataGetNextUpdate(), false, [this](Event::CallbackTimerPtr timer) {
        //    _getWeatherInfo([this](int16_t code, KFCRestAPI::HttpRequest &request) {
        //        __LDBG_printf("code=%d message=%s url=%s", code, __S(request.getMessage()), __S(request.getUrl()));
        //        if (code != 200) {
        //            PrintString message(F("OpenWeatherAPI http status=%d error=%s url=%s"), code, request.getMessage().c_str(), request.getUrl());
        //            Logger_error(message);
        //            _weatherApi.getWeatherInfo().setError(F("OpenWeatherAPI\nHTTP Error"));
        //            _pollDataUpdateLastTime(false);
        //        }
        //        else if (!_weatherApi.getWeatherInfo().hasData()) {
        //            PrintString message(F("OpenWeatherAPI data=invalid message=%s url=%s"), request.getMessage().c_str(), request.getUrl());
        //            Logger_error(message);
        //            _weatherApi.getWeatherInfo().setError(F("OpenWeatherAPI\nInvalid Data"));
        //            _pollDataUpdateLastTime(false);
        //        }
        //        else {
        //            _pollDataUpdateLastTime(true);
        //        }
        //    });
        //});
    }
}

void WeatherStationBase::_pollDataTimerCallback(Event::CallbackTimerPtr timer)
{
}

void WeatherStationBase::_pollDataUpdateLastTime(bool success)
{
    if (success) {
        _pollDataLastMillis = millis();
        if (!_pollDataLastMillis) {
            _pollDataLastMillis++;          // lucky day
        }
        _pollDataRetries = 0;
    }
    else {
        _pollDataLastMillis = 0;            // use kPollDataErrorDelay
        _pollDataRetries++;
    }
}

uint32_t WeatherStationBase::_pollDataGetNextUpdate() const
{
    if (_pollDataLastMillis == 0) {
        return (_pollDataRetries > kPollDataRetries) ? kPollDataErrorPauseDelay : kPollDataErrorDelay;
    }
    uint32_t diff;
    uint32_t next;
    if ((diff = get_time_diff(_pollDataLastMillis, millis())) < Event::minutes(_config.weather_poll_interval).count()) {
        next = (uint32_t)Event::minutes(_config.weather_poll_interval).count() - diff;
    }
    else {
        next = (uint32_t)Event::minutes(_config.weather_poll_interval).count();
    }
    return (next < kMinPollDataInterval) ? kMinPollDataInterval : next;
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

void WeatherStationBase::begin()
{
    // _screenTimer.remove();
    // setScreen(new WeatherStation::Screen::MainScreen());
    _weatherApi.clear();
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, wifiCallback);
    if (WiFi.isConnected()) {
        _wifiCallback(WiFiCallbacks::EventType::CONNECTED, nullptr);
    }
    LoopFunctions::add(loop);
}

void WeatherStationBase::end()
{
    LoopFunctions::remove(loop);
    // _screenTimer.remove();
    _pollDataTimer.remove();
    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, wifiCallback);
    _weatherApi.clear();
}

void WeatherStationBase::_loop()
{
    if (!isCanvasAttached()) {
        return;
    }

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        if (_touchpadDebug) {
            const auto &coords = _touchpad.get();
            uint16_t colorTouched = ST77XX_RED;
            uint16_t colorPredict = ST77XX_YELLOW;
            uint16_t colorGrid = ST77XX_WHITE;
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


    // time_t now = time(nullptr);
    // if (_redrawFlag || _screenLastUpdateTime == ~0) {
    //     MUTEX_LOCK_BLOCK(_lock) {
    //         _redrawFlag = false;
    //     }
    //     //__DBG_printf("redraw=%u update=%d", _redrawFlag, _screenLastUpdateTime);
    //     _canvas->fillScreen(COLORS_BACKGROUND);
    //     _screenLastUpdateTime = (uint32_t)now;
    //     // _screen->drawScreen(*_canvas);
    //     _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
    // }
    // else {
    //     if (_screenLastUpdateTime != now) {
    //         _screenLastUpdateTime = (uint32_t)now;
    //         //__DBG_printf("update_time=%d", _screenLastUpdateTime);
    //         // _screen->updateTime(*_canvas);
    //     }
    //     else {
    //         //__DBG_printf("update_screen=%d", _screenLastUpdateTime);
    //         // _screen->updateScreen(*_canvas);
    //     }
    //     _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
    // }


    // else

    if (_currentScreen == ScreenType::TEXT_CLEAR || _currentScreen == ScreenType::TEXT_UPDATE) {
        _draw();
    }
    else {

        if (_displayMessageTimer) {
            return;
        }

        if (_toggleScreenTimer && millis() > _toggleScreenTimer) {
            // __LDBG_printf("_toggleScreenTimer %u", _toggleScreenTimer);
            _setScreen((_getCurrentScreen() + 1) % kNumScreens);
            _draw();
            return;
        }

        time_t _time = time(nullptr);
        if (_currentScreen < ScreenType::NUM_SCREENS && _lastTime != _time) {
            _updateCounter++;
            do {
                if (/*_currentScreen == ScreenEnum_t::MAIN &&*/ (_updateCounter % 60 == 0)) {
                    // redraw all screens once per minute
                    _draw();
                    break;
                }
                else if (_currentScreen == ScreenType::INDOOR && (_updateCounter % 5 == 0)) {
                    // update indoor section every 5 seconds
                    _updateScreenIndoor();
                }
                else if (_currentScreen == ScreenType::MAIN && (_updateCounter % 5 == 0)) {
                    // update indoor section every 5 seconds
                    _updateWeatherIndoor();
                }

                if (static_cast<uint32_t>(_time - _lastTime) > 10) {
                    // time jumped, redraw everything
                    _draw();
                }
                else {
                    _updateTime();
                }
            } while(false);
        }

    }
}

void WeatherStationBase::_setScreen(ScreenType screen)
{
    if (screen < ScreenType::NUM_SCREENS) {
        auto time = _config.screenTimer[static_cast<uint8_t>(screen)];
        auto next = _getNextScreen(screen);
        // __LDBG_printf("set screen=%u time=%u next=%u", screen, time, next);
        if (time && next != screen) {
            _currentScreen = screen;
            _toggleScreenTimer = millis() + (time * 1000UL);
            return;
        }
        // else ... current screen has no timer or no other screens are available
    }
    // else ... no screen active

    __LDBG_printf("timer removed screen=%u current=%u", screen, _currentScreen);
    _currentScreen = screen;
    _toggleScreenTimer = 0;
}

WeatherStationBase::ScreenType WeatherStationBase::_getNextScreen(ScreenType screen)
{
    for(uint8_t i = static_cast<uint8_t>(screen) + 1; i < static_cast<uint8_t>(screen) + kNumScreens; i++) {
        if (_config.screenTimer[i % kNumScreens]) {
            return static_cast<ScreenType>(i % kNumScreens);
        }
    }
    return screen;
}

