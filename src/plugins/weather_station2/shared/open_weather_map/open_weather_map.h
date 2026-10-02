/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// OpenWeatherMap One Call API 3.0 of the weather station 2.x plugin.
//
// Self-contained on purpose: no KFCOpenWeather, no KFCJson, no LVGL. From the framework only
// Arduino_compat.h is used, for the type of the PROGMEM strings the parser returns. The parser is a
// small streaming reader that walks the response character by character and keeps only the values the
// screens display. No DOM, no temporary buffers - a response of ~16 KB is parsed with ~150 bytes of
// state and never buffered as a whole, so it can be fed from a socket in arbitrary chunks while the
// display keeps running.
//
// Everything is fixed size, so a missing or unexpected field can never allocate: values that do not
// fit into a buffer are truncated, unknown keys and containers are skipped. Only the description is
// a String, the one value that can be long.
//
// Changing the field list (3 steps, nothing to register):
//   1. add the member to Condition/Weather/Hourly/Forecast/Data below
//   2. add the key to the matching switch in Parser::_applyValue() (open_weather_map.cpp)
//   3. nothing else - keys are matched by the FNV-1a hash of their name
//
// https://openweathermap.org/api/one-call-3
//

#include <Arduino_compat.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

// endpoint of the One Call API, change here if the API version changes
#ifndef OPEN_WEATHER_MAP_SCHEME
#    define OPEN_WEATHER_MAP_SCHEME "https"
#endif
#ifndef OPEN_WEATHER_MAP_HOST
#    define OPEN_WEATHER_MAP_HOST "api.openweathermap.org"
#endif
#ifndef OPEN_WEATHER_MAP_PATH
#    define OPEN_WEATHER_MAP_PATH "/data/3.0/onecall"
#endif

namespace WeatherStation2 {

namespace OpenWeatherMap {

// "01d" .. "50n", the longest icon code plus the terminator
static constexpr uint8_t kIconLength = 4;
// "broken clouds", longer descriptions are truncated to this length
static constexpr uint8_t kDescriptionLength = 40;
// error message of the API
static constexpr uint8_t kMessageLength = 64;
// "weather":[{"id":..,"main":..,"description":..,"icon":..}]
static constexpr uint8_t kMaxForecast = 5;
// Entries of the "hourly" array that are kept. The response has 48 of them, the model keeps the
// first kMaxHourly - hourly[0] is the hour of "current.dt", so 25 entries contain every hour of
// the day and the parts of the day of the forecast screen always find their hour
static constexpr uint8_t kMaxHourly = 25;

// one entry of the "weather" array of the current conditions or of a day
struct Condition {
    // 2xx thunderstorm, 3xx drizzle, 5xx rain, 6xx snow, 7xx atmosphere, 800 clear, 801.. clouds
    uint16_t id;
    char icon[kIconLength];
    String description;

    void clear() {
        id = 0;
        icon[0] = 0;
        description = "";
    }
};

// the "current" object of the response
struct Weather {
    time_t dt;
    // sunrise/sunset of the day, unix time (UTC)
    time_t sunrise;
    time_t sunset;
    float temperature;
    float feelsLike;
    float pressure;
    float humidity;
    float windSpeed;
    // rain of the last hour in mm
    float rain;
    float uvIndex;
    Condition condition;

    void clear() {
        *this = Weather();
        condition.clear();
    }

    Weather() :
        dt(0), sunrise(0), sunset(0),
        temperature(0), feelsLike(0), pressure(0), humidity(0), windSpeed(0), rain(0), uvIndex(0)
    {
    }
};

// one entry of the "hourly" array. The 1 day layout of the forecast screen displays the four
// parts of the day from it, so every part has its own icon. The description of the condition is
// not kept, only the icon is displayed
struct Hourly {
    time_t dt;
    float temperature;
    float feelsLike;
    // rain of the hour in mm
    float rain;
    // probability of precipitation, 0..1
    float pop;
    // 2xx thunderstorm .. 8xx clouds of "weather"[0], with the icon code ("01d".."50n")
    uint16_t conditionId;
    char icon[kIconLength];

    void clear() {
        *this = Hourly();
    }

    Hourly() :
        dt(0), temperature(0), feelsLike(0), rain(0), pop(0), conditionId(0), icon{}
    {
    }
};

// one entry of the "daily" array
struct Forecast {
    time_t dt;
    float minTemperature;
    float maxTemperature;
    // rain of the day in mm
    float rain;
    Condition condition;

    void clear() {
        *this = Forecast();
        condition.clear();
    }

    Forecast() :
        dt(0), minTemperature(0), maxTemperature(0), rain(0)
    {
    }
};

// everything the screens need from one response
struct Data {
    // true after a complete response was parsed successfully
    bool valid;
    // offset of the location to UTC in seconds (the API sends the timezone of the coordinates,
    // not the timezone of the device)
    int32_t timezoneOffset;
    Weather current;
    Hourly hourly[kMaxHourly];
    Forecast forecast[kMaxForecast];
    uint8_t forecastCount;

    void clear() {
        valid = false;
        timezoneOffset = 0;
        current.clear();
        for (auto &hour : hourly) {
            hour.clear();
        }
        for (auto &day : forecast) {
            day.clear();
        }
        forecastCount = 0;
    }

    Data() {
        clear();
    }
};

// parameters of the request
struct Settings {
    // API key of the account (required)
    const char *apiKey;
    double latitude;
    double longitude;
    // metric (Celsius, m/s, mm) or imperial (Fahrenheit, mph, in)
    bool metric;
    // language of the description, nullptr uses the default of the API (English)
    const char *language;
};

// builds the request URL into a caller supplied buffer, returns its length or 0 if it does not fit
size_t buildRequestUrl(const Settings &settings, char *buffer, size_t size);

// streaming reader for a One Call response. One instance can be reused for every request (reset())
class Parser {
public:
    // containers that are tracked, deeper values are skipped (the response nests up to 5)
    static constexpr uint8_t kMaxDepth = 8;
    // longest key that is kept, longer values are truncated
    static constexpr uint8_t kTokenLength = kMessageLength + 8;

    Parser(Data &data);

    // forget everything, the object can be used for the next response
    void reset();

    // feeds one chunk of the response, returns false on a fatal error (see getError())
    bool feed(const char *data, size_t length);

    // ends the stream, returns false if the response is incomplete or the API reported an error
    bool end();

    // values of the last completed response
    const Data &getData() const {
        return _data;
    }

    // Error of the response as a PROGMEM string - read it with a flash aware reader (String,
    // Print::print, ...). nullptr while no error occurred. When the API answered with an error it
    // reports the reason in its message, getMessage() is more specific then
    const __FlashStringHelper *getError() const {
        return _error;
    }

    // "message" of the response, runtime data of the API (RAM), empty when the response has none
    const String &getMessage() const {
        return _message;
    }

private:
    // what the next token is
    enum class Next : uint8_t {
        KEY,
        VALUE,
        COLON,
        COMMA,
        END,
    };

    // what the last completed value was
    enum class Type : uint8_t {
        NONE,
        NUMBER,
        STRING,
    };

    struct Level {
        // hash of the key of the container inside its parent (0 for the root and for array elements)
        uint32_t key;
        // index of the array element (-1 when it is not an array or an array element)
        int16_t index;
        bool isArray;
    };

    // FNV-1a, used as a case label to match the keys without storing any strings
    static constexpr uint32_t hash(const char *key)
    {
        uint32_t value = 2166136261u;
        while (*key) {
            value = (value ^ static_cast<uint8_t>(*key++)) * 16777619u;
        }
        return value;
    }

    bool _push(bool isArray);
    void _pop();
    void _advance();
    void _append(char c);
    void _appendUtf8(uint16_t code);
    void _stringEnd();
    void _tokenEnd();
    void _finishValue();
    void _applyValue();
    void _applyCondition(Condition &condition);
    // stores the first error, the values passed in are PROGMEM strings (F() literals)
    void _setError(const __FlashStringHelper *error);
    void _setCode(int32_t code);
    // copies a completed string value into a fixed buffer or a String field
    void _copy(char *destination, size_t size);
    void _copy(String &destination, uint8_t maxLength);
    uint32_t _keyAt(uint8_t distance) const;
    int16_t _indexAt(uint8_t distance) const;

    Data &_data;
    Level _level[kMaxDepth];
    uint8_t _depth;
    Next _next;
    Type _type;
    String _token;
    bool _tokenOverflow;
    bool _inString;
    bool _inToken;
    bool _escape;
    bool _inUnicode;
    uint8_t _unicodeDigits;
    uint16_t _unicode;
    uint32_t _keyHash;
    double _number;
    int32_t _code;
    String _message;
    uint16_t _hasCurrent;
    const __FlashStringHelper *_error;
};

} // namespace OpenWeatherMap

} // namespace WeatherStation2
