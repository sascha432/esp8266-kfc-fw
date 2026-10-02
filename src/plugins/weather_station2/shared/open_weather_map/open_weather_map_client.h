/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "open_weather_map.h"

//
// HTTP client of the OpenWeatherMap module: one request, streamed into the parser, plus the mapping
// of the parsed values into the data model of the screens.
//
// The request blocks while the response is transferred (~0.5-2 s with TLS), so it must not run while
// the UI is drawing. Either call it from a task or at a moment where a short stall does not matter
// (e.g. right after the data was displayed), with at least Client::kUpdateInterval between requests.
//

// 1 = request over TLS (WiFiClientSecure). The One Call API 3.0 requires https, only change this
// together with OPEN_WEATHER_MAP_SCHEME in open_weather_map.h. Disabling it saves ~40 KB of heap
// during the request but the API will not answer over plain http.
#ifndef OPEN_WEATHER_MAP_USE_TLS
#    define OPEN_WEATHER_MAP_USE_TLS 1
#endif

#include "global.h"

#include <Arduino_compat.h>
#include "../../ws2_data.h"

namespace WeatherStation2 {

namespace OpenWeatherMap {

class Client {
public:
    // Connect/transfer timeout of one request. The response is ~20 KB because the hourly section
    // is requested (the four parts of the day of the forecast screen), the transfer alone can
    // take a few seconds on a busy WiFi
    static constexpr uint32_t kTimeout = 15000;
    // suggested interval between two requests. The API allows 60/minute, the free plan 1000/day,
    // the screens do not need an update faster than this
    static constexpr uint32_t kUpdateInterval = 10 * 60 * 1000;
    // read buffer, the response is parsed while it arrives and is never buffered as a whole
    static constexpr size_t kBufferSize = 512;
    // space for the request url
    static constexpr size_t kUrlLength = 256;

    Client();

    // The strings of the settings are not copied, they must stay valid while the client is used
    // (the configuration keeps them in memory, so passing the config strings is fine)
    void setSettings(const Settings &settings) {
        _settings = settings;
    }
    const Settings &getSettings() const {
        return _settings;
    }
    // true when an API key and a location are set
    bool hasSettings() const {
        return _settings.apiKey && *_settings.apiKey;
    }

    // performs one request and parses it into data. Blocking, see the note above
    bool fetch(Data &data);

    // error of the last request, empty when it succeeded ("HTTP 401: Invalid API key ...")
    const String &getError() const {
        return _error;
    }
    // duration of the last request in milliseconds
    uint32_t getDuration() const {
        return _duration;
    }
    // HTTP status code of the last request
    int getStatusCode() const {
        return _statusCode;
    }
    // number of requests since the last reset
    uint32_t getRequestCount() const {
        return _requestCount;
    }

private:
    Settings _settings;
    String _error;
    uint32_t _duration;
    uint32_t _requestCount;
    int _statusCode;
};

// icon of a condition, the id of the API is 2xx thunderstorm, 3xx/5xx rain, 6xx snow, 7xx
// atmosphere, 800 clear, 801..804 clouds. Falls back to the icon code ("01d".."50n") without an id
WeatherIcon toWeatherIcon(uint16_t id, const char *icon);

// maps the current conditions into the model of the main screen
void applyCurrent(const Data &data, CurrentWeather &current);

// maps the daily forecast into the model of the forecast screen, returns the number of days.
// The weekday is derived from the timezone of the location, so it does not depend on the clock
// or the timezone of the device
uint8_t applyForecast(const Data &data, ForecastDay *forecast, uint8_t max);

// Maps the hourly forecast into the four parts of the day of the 1 day layout of the forecast
// screen (morning, noon, afternoon, night, see getDayPartHour()). Every part is the hourly entry
// closest to its hour of the day, so each part has its own icon and its own values. The time of
// the slot is the local time of that entry and carries the weekday when it is not the day of
// "current". Returns the number of slots that were filled
uint8_t applyDayParts(const Data &data, ForecastSlot *dayParts, uint8_t max);

} // namespace OpenWeatherMap

} // namespace WeatherStation2
