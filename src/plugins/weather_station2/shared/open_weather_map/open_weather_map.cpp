/**
 * Author: sascha_lammers@gmx.de
 */

#include "open_weather_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Flash strings: every string this file hands out is a PROGMEM string (`const __FlashStringHelper *`)
// and is read with pgm_read_byte by String/Print - the error messages are F() literals assigned to
// _error and the request URL is a PSTR() argument of snprintf_P, both put the literal into the flash
// segment. The pairing matters: the format string is read by the flash aware printf, whereas the
// *arguments* of %s are read with a plain pointer and therefore stay plain literals.

namespace WeatherStation2 {

namespace OpenWeatherMap {

size_t buildRequestUrl(const Settings &settings, char *buffer, size_t size)
{
    if (!settings.apiKey || !*settings.apiKey || size < 64) {
        return 0;
    }
    // "metric"/"imperial" are passed as a %s argument, see the note above
    const char *units = settings.metric ? "metric" : "imperial";
    const char *language = (settings.language && *settings.language) ? settings.language : nullptr;

    // "exclude" keeps the response small: the minute forecast is not displayed and the alerts
    // are unused. "hourly" is requested, the four parts of the day of the forecast screen come
    // from it (~20 KB, the request already has a longer timeout for it)
    const int length = snprintf_P(buffer, size, PSTR(
        OPEN_WEATHER_MAP_SCHEME "://" OPEN_WEATHER_MAP_HOST OPEN_WEATHER_MAP_PATH
        "?lat=%.4f&lon=%.4f&units=%s&exclude=minutely,alerts&appid=%s%s%s"),
        settings.latitude, settings.longitude, units, settings.apiKey,
        language ? "&lang=" : "", language ? language : "");

    if (length < 0 || static_cast<size_t>(length) >= size) {
        return 0;
    }
    return static_cast<size_t>(length);
}

// ------------------------------------------------------------------------------------------
// helpers
// ------------------------------------------------------------------------------------------

static inline bool _isTokenChar(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '-' || c == '+' || c == '.' || c == '_';
}

static inline int _hexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// ------------------------------------------------------------------------------------------
// Parser
// ------------------------------------------------------------------------------------------

Parser::Parser(Data &data) : _data(data)
{
    reset();
}

void Parser::reset()
{
    _depth = 0;
    _next = Next::VALUE;
    _type = Type::NONE;
    _token = "";
    _tokenOverflow = false;
    _inString = false;
    _inToken = false;
    _escape = false;
    _inUnicode = false;
    _unicodeDigits = 0;
    _unicode = 0;
    _keyHash = 0;
    _number = 0;
    _code = 0;
    _message = "";
    _hasCurrent = 0;
    _error = nullptr;
    for (auto &level : _level) {
        level.key = 0;
        level.index = -1;
        level.isArray = false;
    }
    _data.clear();
}

bool Parser::feed(const char *data, size_t length)
{
    while (length--) {
        const char c = *data++;

        // --- inside a string, the token is either a key or a value
        if (_inString) {
            if (_escape) {
                _escape = false;
                switch (c) {
                case 'n':
                    _append('\n');
                    break;
                case 't':
                    _append('\t');
                    break;
                case 'r':
                    _append('\r');
                    break;
                case 'b':
                    _append('\b');
                    break;
                case 'f':
                    _append('\f');
                    break;
                case 'u':
                    _inUnicode = true;
                    _unicodeDigits = 0;
                    _unicode = 0;
                    break;
                default: // covers the escaped quote, the backslash and the slash
                    _append(c);
                    break;
                }
                continue;
            }
            if (_inUnicode) {
                const auto digit = _hexDigit(c);
                if (digit < 0) {
                    _inUnicode = false;
                    _append('?');
                    continue;
                }
                _unicode = static_cast<uint16_t>((_unicode << 4) | digit);
                if (++_unicodeDigits >= 4) {
                    _inUnicode = false;
                    _appendUtf8(_unicode);
                }
                continue;
            }
            switch (c) {
            case '\\':
                _escape = true;
                break;
            case '"':
                _inString = false;
                _stringEnd();
                break;
            default:
                _append(c);
                break;
            }
            continue;
        }

        // --- a bare token (number, true, false, null) ends at the next delimiter
        if (_inToken) {
            if (_isTokenChar(c)) {
                _append(c);
                continue;
            }
            _tokenEnd(); // the delimiter is processed below
        }

        switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            break;

        case '{':
            if (!_push(false)) {
                return false;
            }
            _next = Next::KEY;
            break;

        case '[':
            if (!_push(true)) {
                return false;
            }
            _next = Next::VALUE;
            break;

        case '}':
        case ']':
            _pop();
            break;

        case ':':
            _next = Next::VALUE;
            break;

        case ',':
            _next = (_depth && _level[_depth - 1].isArray) ? Next::VALUE : Next::KEY;
            break;

        case '"':
            _inString = true;
            _token = "";
            _tokenOverflow = false;
            break;

        default:
            if (_isTokenChar(c)) {
                _inToken = true;
                _token = "";
                _tokenOverflow = false;
                _append(c);
            }
            else {
                _setError(F("unexpected character"));
                return false;
            }
            break;
        }
    }
    return _error == nullptr;
}

bool Parser::end()
{
    if (_error) {
        return false;
    }
    if (_inString || _inToken || _depth != 0) {
        _setError(F("truncated response"));
        return false;
    }
    if (_code != 0) {
        // the reason is reported in the message of the API (getMessage())
        _setError(F("the API reported an error"));
        return false;
    }
    if (!_hasCurrent) {
        _setError(F("no weather data in the response"));
        return false;
    }
    _data.valid = true;
    return true;
}

// ------------------------------------------------------------------------------------------
// state machine
// ------------------------------------------------------------------------------------------

bool Parser::_push(bool isArray)
{
    if (_depth >= kMaxDepth) {
        _setError(F("too deeply nested"));
        return false;
    }
    auto &level = _level[_depth];
    level.isArray = isArray;
    level.index = -1;
    level.key = 0;
    if (_depth) {
        // an array element inherits the key of the array and continues its numbering (the index of
        // the array is the last completed element), an object member takes the key of the value
        const auto &parent = _level[_depth - 1];
        if (parent.isArray) {
            level.key = parent.key;
            level.index = static_cast<int16_t>(parent.index + 1);
        }
        else {
            level.key = _keyHash;
        }
    }
    _depth++;
    return true;
}

void Parser::_pop()
{
    if (_depth == 0) {
        _setError(F("unbalanced braces"));
        return;
    }
    _depth--;
    _advance();
}

void Parser::_advance()
{
    if (_depth == 0) {
        _next = Next::END;
        return;
    }
    if (_level[_depth - 1].isArray) {
        _level[_depth - 1].index++;
    }
    _next = Next::COMMA;
}

void Parser::_append(char c)
{
    if (_token.length() < kTokenLength - 1) {
        _token += c;
    }
    else {
        _tokenOverflow = true;
    }
}

void Parser::_appendUtf8(uint16_t code)
{
    if (code < 0x80) {
        _append(static_cast<char>(code));
    }
    else if (code < 0x800) {
        _append(static_cast<char>(0xc0 | (code >> 6)));
        _append(static_cast<char>(0x80 | (code & 0x3f)));
    }
    else {
        _append(static_cast<char>(0xe0 | (code >> 12)));
        _append(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
        _append(static_cast<char>(0x80 | (code & 0x3f)));
    }
}

void Parser::_stringEnd()
{
    if (_next == Next::KEY) {
        // a truncated key cannot be matched, the value is skipped (a hash of 0 matches nothing)
        _keyHash = _tokenOverflow ? 0 : hash(_token.c_str());
        _next = Next::COLON;
        return;
    }
    _type = Type::STRING;
    _finishValue();
}

void Parser::_tokenEnd()
{
    _inToken = false;
    if (_next == Next::KEY) {
        _keyHash = _tokenOverflow ? 0 : hash(_token.c_str());
        _next = Next::COLON;
        return;
    }
    if (_tokenOverflow || _token.isEmpty()) {
        _type = Type::NONE;
    }
    else if (_token.equals("null")) {
        _type = Type::NONE;
    }
    else if (_isTokenChar(_token.charAt(0)) && (_token.charAt(0) == '-' || _token.charAt(0) == '+' || _token.charAt(0) == '.' || (_token.charAt(0) >= '0' && _token.charAt(0) <= '9'))) {
        _number = strtod(_token.c_str(), nullptr);
        _type = Type::NUMBER;
    }
    else {
        _type = Type::NONE; // true/false, not displayed
    }
    _finishValue();
}

void Parser::_finishValue()
{
    // a value of an object member is dispatched, an array element only advances the index
    if (_depth && !_level[_depth - 1].isArray) {
        _applyValue();
    }
    _advance();
}

void Parser::_copy(char *destination, size_t size)
{
    if (_type != Type::STRING) {
        return;
    }
    const size_t length = (_token.length() < size - 1) ? _token.length() : size - 1;
    memcpy(destination, _token.c_str(), length);
    destination[length] = 0;
}

void Parser::_copy(String &destination, uint8_t maxLength)
{
    if (_type != Type::STRING) {
        return;
    }
    destination = _token;
    if (destination.length() > maxLength) {
        destination.remove(maxLength);
    }
}

// ------------------------------------------------------------------------------------------
// field mapping - the JSON path is (container, parent, grand parent) + the key of the value
// ------------------------------------------------------------------------------------------

void Parser::_applyCondition(Condition &condition)
{
    switch (_keyHash) {
    case hash("id"):
        condition.id = static_cast<uint16_t>(_number);
        break;
    case hash("icon"):
        _copy(condition.icon, kIconLength);
        break;
    case hash("description"):
        _copy(condition.description, kDescriptionLength);
        break;
    default:
        break;
    }
}

void Parser::_applyValue()
{
    if (_type == Type::NONE) {
        return;
    }
    const uint32_t container = _keyAt(1);
    const uint32_t parent = _keyAt(2);
    const uint32_t grandParent = _keyAt(3);

    switch (container) {
    case hash("current"): // current.<key>
        switch (_keyHash) {
        case hash("dt"):
            _data.current.dt = static_cast<time_t>(_number);
            _hasCurrent++;
            break;
        case hash("sunrise"):
            _data.current.sunrise = static_cast<time_t>(_number);
            break;
        case hash("sunset"):
            _data.current.sunset = static_cast<time_t>(_number);
            break;
        case hash("temp"):
            _data.current.temperature = static_cast<float>(_number);
            break;
        case hash("feels_like"):
            _data.current.feelsLike = static_cast<float>(_number);
            break;
        case hash("pressure"):
            _data.current.pressure = static_cast<float>(_number);
            break;
        case hash("humidity"):
            _data.current.humidity = static_cast<float>(_number);
            break;
        case hash("wind_speed"):
            _data.current.windSpeed = static_cast<float>(_number);
            break;
        case hash("uvi"):
            _data.current.uvIndex = static_cast<float>(_number);
            break;
        default:
            break;
        }
        break;

    case hash("rain"): // current.rain.1h / hourly[i].rain.1h
        if (_keyHash != hash("1h")) {
            break;
        }
        if (parent == hash("current")) {
            _data.current.rain = static_cast<float>(_number);
        }
        else if (parent == hash("hourly")) {
            // the entry of the rain object, the hourly index is two levels up (the rain object
            // itself is not an array element)
            const auto hour = _indexAt(2);
            if (hour >= 0 && hour < kMaxHourly) {
                _data.hourly[hour].rain = static_cast<float>(_number);
            }
        }
        break;

    case hash("temp"): // daily[i].temp.min / daily[i].temp.max
        if (parent == hash("daily")) {
            const auto day = _indexAt(2);
            if (day >= 0 && day < kMaxForecast) {
                if (_keyHash == hash("min")) {
                    _data.forecast[day].minTemperature = static_cast<float>(_number);
                }
                else if (_keyHash == hash("max")) {
                    _data.forecast[day].maxTemperature = static_cast<float>(_number);
                }
            }
        }
        break;

    case hash("weather"): // current.weather[0].<key> / daily[i].weather[0].<key> / hourly[i].weather[0].<key>
        // only the first entry is used, the API repeats the same condition with different icon sizes
        if (parent == hash("weather") && _indexAt(1) == 0) {
            if (grandParent == hash("current")) {
                _applyCondition(_data.current.condition);
            }
            else if (grandParent == hash("daily")) {
                const auto day = _indexAt(3);
                if (day >= 0 && day < kMaxForecast) {
                    _applyCondition(_data.forecast[day].condition);
                }
            }
            else if (grandParent == hash("hourly")) {
                // the description of an hourly entry is not displayed, only the id and the icon
                const auto hour = _indexAt(3);
                if (hour >= 0 && hour < kMaxHourly) {
                    auto &entry = _data.hourly[hour];
                    if (_keyHash == hash("id")) {
                        entry.conditionId = static_cast<uint16_t>(_number);
                    }
                    else if (_keyHash == hash("icon")) {
                        _copy(entry.icon, kIconLength);
                    }
                }
            }
        }
        break;

    case hash("daily"): // daily[i].<key>
        if (parent == hash("daily")) {
            const auto day = _indexAt(1);
            if (day >= 0 && day < kMaxForecast) {
                if (_keyHash == hash("dt")) {
                    _data.forecast[day].dt = static_cast<time_t>(_number);
                    if (_data.forecastCount <= day) {
                        _data.forecastCount = static_cast<uint8_t>(day + 1);
                    }
                }
                else if (_keyHash == hash("rain")) {
                    _data.forecast[day].rain = static_cast<float>(_number);
                }
            }
        }
        break;

    case hash("hourly"): // hourly[i].<key>
        if (parent == hash("hourly")) {
            // the array has 48 entries, only the first kMaxHourly are kept (their rain object is
            // handled above)
            const auto hour = _indexAt(1);
            if (hour >= 0 && hour < kMaxHourly) {
                auto &entry = _data.hourly[hour];
                switch (_keyHash) {
                case hash("dt"):
                    entry.dt = static_cast<time_t>(_number);
                    break;
                case hash("temp"):
                    entry.temperature = static_cast<float>(_number);
                    break;
                case hash("feels_like"):
                    entry.feelsLike = static_cast<float>(_number);
                    break;
                case hash("pop"):
                    entry.pop = static_cast<float>(_number);
                    break;
                default:
                    break;
                }
            }
        }
        break;

    case 0: // root
        if (_keyHash == hash("timezone_offset")) {
            _data.timezoneOffset = static_cast<int32_t>(_number);
        }
        else if (_keyHash == hash("cod")) {
            // an error response is {"cod":401,"message":"..."} or {"cod":"401","message":"..."}
            if (_type == Type::NUMBER) {
                _setCode(static_cast<int32_t>(_number));
            }
            else if (_type == Type::STRING) {
                _setCode(static_cast<int32_t>(strtol(_token.c_str(), nullptr, 10)));
            }
        }
        else if (_keyHash == hash("message")) {
            _copy(_message, kMessageLength);
        }
        break;

    default:
        break;
    }
}

void Parser::_setCode(int32_t code)
{
    if (code != 0 && code != 200) {
        _code = code;
    }
}

void Parser::_setError(const __FlashStringHelper *error)
{
    // the messages are PROGMEM strings and are handed out by getError(), the caller reads them with a
    // flash aware reader (String assignment, Print::print, ...) - see the note at the top of the file
    if (!_error) {
        _error = error;
    }
    _data.valid = false;
}

uint32_t Parser::_keyAt(uint8_t distance) const
{
    return (distance >= 1 && distance <= _depth) ? _level[_depth - distance].key : 0;
}

int16_t Parser::_indexAt(uint8_t distance) const
{
    return (distance >= 1 && distance <= _depth) ? _level[_depth - distance].index : -1;
}

} // namespace OpenWeatherMap

} // namespace WeatherStation2
