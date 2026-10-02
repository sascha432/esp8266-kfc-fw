/**
 * Author: sascha_lammers@gmx.de
 */

#include "open_weather_map_client.h"

#include <HTTPClient.h>
#include <PrintString.h>
#include <WiFiClient.h>
#if OPEN_WEATHER_MAP_USE_TLS
#    include <WiFiClientSecure.h>
#endif

#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 1
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WeatherStation2 {

namespace OpenWeatherMap {

Client::Client() : _error(), _duration(0), _requestCount(0), _statusCode(0)
{
    _settings.apiKey = nullptr;
    _settings.latitude = 0;
    _settings.longitude = 0;
    _settings.metric = true;
    _settings.language = nullptr;
}

bool Client::fetch(Data &data)
{
    _error = String();
    _statusCode = 0;
    _duration = 0;

    char url[kUrlLength];
    if (buildRequestUrl(_settings, url, sizeof(url)) == 0) {
        _error = F("no api key or invalid location configured");
        return false;
    }

    const auto started = millis();

#if ESP32
    // The TLS handshake needs ~40 KB of heap, the internal DRAM is the scarce resource of this
    // board (the mbedtls allocator is switched to the PSRAM during the plugin setup). Without it
    // the X.509 parser fails with "X509 - Allocation of memory failed".
    __LDBG_printf("request starting (heap=%u, psram=%u)", ESP.getFreeHeap(), ESP.getFreePsram());
#endif

#if OPEN_WEATHER_MAP_USE_TLS
    WiFiClientSecure client;
    // the device has no certificate store and the API does not use a pinned certificate
    client.setInsecure();
#else
    WiFiClient client;
#endif

    HTTPClient http;
    if (!http.begin(client, url)) {
        _error = F("cannot connect to the API");
        _duration = millis() - started;
        return false;
    }
    http.setTimeout(kTimeout);
    http.setConnectTimeout(kTimeout);
    http.setReuse(false);

    const auto status = http.GET();
    _statusCode = status;

    // parse the response while it arrives, the whole body is never buffered (~16 KB)
    Parser parser(data);
    auto stream = http.getStreamPtr();
    const auto contentLength = http.getSize();
    uint8_t buffer[kBufferSize];
    uint32_t received = 0;
    bool complete = true;
    const auto deadline = started + kTimeout;

    while (stream) {
        const auto available = stream->available();
        if (available > 0) {
            const auto size = (static_cast<size_t>(available) > sizeof(buffer)) ? sizeof(buffer) : static_cast<size_t>(available);
            const auto length = stream->readBytes(buffer, size);
            if (length == 0) {
                break;
            }
            received += length;
            if (!parser.feed(reinterpret_cast<const char *>(buffer), length)) {
                break;
            }
            if (contentLength >= 0 && received >= static_cast<uint32_t>(contentLength)) {
                break;
            }
            continue;
        }
        if (contentLength >= 0 && received >= static_cast<uint32_t>(contentLength)) {
            break;
        }
        if (!http.connected()) {
            break;
        }
        if (static_cast<int32_t>(millis() - deadline) >= 0) {
            complete = false;
            break;
        }
        delay(1);
    }
    http.end();

    _duration = millis() - started;
    _requestCount++;

    // "cod" of the API is part of the error responses, so a 401 answers with a parsable message
    if (!complete || !parser.end()) {
        // the message of the API is more specific than the error of the parser
        const auto &message = parser.getMessage();
        if (!message.isEmpty()) {
            _error = message;
        }
        else if (const auto error = parser.getError()) {
            _error = error;
        }
        else {
            _error = F("incomplete response");
        }
        if (status != HTTP_CODE_OK) {
            // 401/429 report the reason in the body, keep the status code in front of it
            _error = PrintString(F("HTTP %d: %s"), status, _error.c_str());
        }
        __LDBG_printf("request failed after %ums (status=%d, %u bytes): %s", _duration, status, received, _error.c_str());
#if ESP32
        __LDBG_printf("heap=%u, psram=%u", ESP.getFreeHeap(), ESP.getFreePsram());
#endif
        return false;
    }

    __LDBG_printf("request ok after %ums (%u bytes, %u forecast days, %s, %.1f C, %.1f%%)", _duration, received,
                  parser.getData().forecastCount, parser.getData().current.condition.description.c_str(),
                  parser.getData().current.temperature, parser.getData().current.humidity);
    return true;
}

// ------------------------------------------------------------------------------------------
// mapping into the model of the screens
// ------------------------------------------------------------------------------------------

WeatherIcon toWeatherIcon(uint16_t id, const char *icon)
{
    switch (id / 100) {
    case 2:
        return WeatherIcon::STORM;
    case 3:
    case 5:
        return WeatherIcon::RAIN;
    case 6:
        return WeatherIcon::SNOW;
    case 7:
        return WeatherIcon::FOG;
    case 8:
        if (id == 800) {
            return WeatherIcon::SUN;
        }
        return (id == 801) ? WeatherIcon::PARTLY_CLOUDY : WeatherIcon::CLOUDY;
    default:
        break;
    }
    // without an id, use the condition group of the icon code ("01d" .. "50n")
    if (icon && icon[0]) {
        switch (icon[0]) {
        case '0':
            switch (icon[1]) {
            case '1':
                return WeatherIcon::SUN;
            case '2':
                return WeatherIcon::PARTLY_CLOUDY;
            default:
                return WeatherIcon::CLOUDY;
            }
        case '1':
            switch (icon[1]) {
            case '0':
                return WeatherIcon::RAIN;
            case '1':
                return WeatherIcon::STORM;
            case '3':
                return WeatherIcon::SNOW;
            default:
                break;
            }
            break;
        case '5':
            if (icon[1] == '0') {
                return WeatherIcon::FOG;
            }
            break;
        default:
            break;
        }
    }
    return WeatherIcon::UNKNOWN;
}

// minutes since midnight of a timestamp that already includes the timezone offset
static int16_t _minutesOfDay(time_t value)
{
    auto seconds = static_cast<uint32_t>(value % 86400);
    return static_cast<int16_t>(seconds / 60);
}

void applyCurrent(const Data &data, CurrentWeather &current)
{
    const auto &weather = data.current;
    current.valid = true;
    current.icon = toWeatherIcon(weather.condition.id, weather.condition.icon);
    current.description = weather.condition.description;
    current.temperature = weather.temperature;
    current.feelsLike = weather.feelsLike;

    // the One Call API 3.0 has no min/max for the current conditions, the first day is the current one
    if (data.forecastCount) {
        current.minTemperature = data.forecast[0].minTemperature;
        current.maxTemperature = data.forecast[0].maxTemperature;
    }
    current.humidity = weather.humidity;
    current.pressure = weather.pressure;
    current.windSpeed = weather.windSpeed;
    current.rain = weather.rain;
    current.uvIndex = weather.uvIndex;

    // the API sends the sunrise/sunset of the location as UTC, the offset is the one of the location
    const auto offset = static_cast<time_t>(data.timezoneOffset);
    current.sunRise = weather.sunrise ? _minutesOfDay(weather.sunrise + offset) : -1;
    current.sunSet = weather.sunset ? _minutesOfDay(weather.sunset + offset) : -1;
}

// weekday of a UTC timestamp that already includes the timezone offset (1970-01-01 was a Thursday)
static const __FlashStringHelper *_weekday(time_t value)
{
    auto index = static_cast<int32_t>((value / 86400 + 4) % 7);
    if (index < 0) {
        index += 7;
    }
    switch (index) {
    case 0:
        return F("Sun");
    case 1:
        return F("Mon");
    case 2:
        return F("Tue");
    case 3:
        return F("Wed");
    case 4:
        return F("Thu");
    case 5:
        return F("Fri");
    default: // 6
        return F("Sat");
    }
}

uint8_t applyForecast(const Data &data, ForecastDay *forecast, uint8_t max)
{
    const auto count = (data.forecastCount < max) ? data.forecastCount : max;
    const auto offset = static_cast<time_t>(data.timezoneOffset);
    for (uint8_t i = 0; i < count; i++) {
        const auto &day = data.forecast[i];
        forecast[i].valid = true;
        forecast[i].day = day.dt ? _weekday(day.dt + offset) : String();
        forecast[i].icon = toWeatherIcon(day.condition.id, day.condition.icon);
        forecast[i].minTemperature = day.minTemperature;
        forecast[i].maxTemperature = day.maxTemperature;
        forecast[i].rain = day.rain;
    }
    return count;
}

// hour of the day of a local timestamp, the date part is dropped
static uint8_t _hourOfDay(time_t local)
{
    return static_cast<uint8_t>((static_cast<uint32_t>(local % 86400) / 3600) % 24);
}

// "09:00" of a local timestamp
static void _timeOfDay(time_t local, char *buffer, size_t size)
{
    const auto seconds = static_cast<uint32_t>(local % 86400);
    snprintf(buffer, size, "%02u:%02u", static_cast<unsigned>(seconds / 3600), static_cast<unsigned>((seconds / 60) % 60));
}

uint8_t applyDayParts(const Data &data, ForecastSlot *dayParts, uint8_t max)
{
    const auto count = (kNumDayParts < max) ? kNumDayParts : max;
    const auto offset = static_cast<time_t>(data.timezoneOffset);
    // local day of "now" of the response, the weekday is only shown for a slot of another day
    const auto currentDay = static_cast<int32_t>((data.current.dt + offset) / 86400);
    uint8_t filled = 0;

    for (uint8_t i = 0; i < count; i++) {
        auto &slot = dayParts[i];
        slot = ForecastSlot();

        // The entry closest to the hour of this part of the day. The entries start at the current
        // hour, so an hour that is already over today is taken from the next day and the time of
        // the slot reports the day it belongs to. The first of two equally close entries wins,
        // which is the earlier one
        const Hourly *entry = nullptr;
        uint8_t distance = 24;
        const auto partHour = getDayPartHour(i);
        for (uint8_t j = 0; j < kMaxHourly; j++) {
            if (!data.hourly[j].dt) {
                continue;
            }
            const auto hour = _hourOfDay(data.hourly[j].dt + offset);
            const auto delta = static_cast<uint8_t>((hour > partHour) ? (hour - partHour) : (partHour - hour));
            const auto current = static_cast<uint8_t>((delta < 12) ? delta : (24 - delta));
            if (current < distance) {
                distance = current;
                entry = &data.hourly[j];
            }
        }
        if (!entry) {
            continue;
        }

        const auto local = entry->dt + offset;
        char time[8];
        _timeOfDay(local, time, sizeof(time));
        if (static_cast<int32_t>(local / 86400) != currentDay) {
            // the part of the day belongs to another day, the weekday makes that visible
            slot.time = String(_weekday(local)) + " " + time;
        }
        else {
            slot.time = time;
        }
        slot.valid = true;
        slot.icon = toWeatherIcon(entry->conditionId, entry->icon);
        slot.temperature = entry->temperature;
        slot.feelsLike = entry->feelsLike;
        slot.rain = entry->rain;
        slot.pop = entry->pop;
        filled++;
    }
    return filled;
}

} // namespace OpenWeatherMap

} // namespace WeatherStation2
