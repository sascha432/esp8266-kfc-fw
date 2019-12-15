/**
* Author: sascha_lammers@gmx.de
*/

#include "OpenWeatherInfoJsonReader.h"

OpenWeatherInfoJsonReader::OpenWeatherInfoJsonReader(Stream * stream, OpenWeatherMapAPI::WeatherInfo & info) : JsonBaseReader(stream), _info(info) {
}

OpenWeatherInfoJsonReader::OpenWeatherInfoJsonReader(OpenWeatherMapAPI::WeatherInfo & info) : OpenWeatherInfoJsonReader(nullptr, info) {
}

bool OpenWeatherInfoJsonReader::beginObject(bool isArray)
{
    if (!strcmp_P(getObjectPath(false).c_str(), PSTR("weather[]"))) {
        _info.weather.emplace_back(OpenWeatherMapAPI::Weather_t());
    }
    //auto pathStr = getObjectPath(false);
    //auto path = pathStr.c_str();
    //Serial.printf("begin path %s array=%u\n", path, isArray);
    return true;
}

bool OpenWeatherInfoJsonReader::processElement() {

    auto keyStr = getKey();
    auto key = keyStr.c_str();
    auto pathStr = getPath(false);
    auto path = pathStr.c_str();

    //Serial.printf("key %s value %s type %s path %s index %d\n", key, getValue().c_str(), jsonType2String(getType()).c_str(), path, getObjectIndex());

    if (!strcmp_P(path, PSTR("name"))) {
        _info.location = getValue();
    }
    else if (!strcmp_P(path, PSTR("visibility"))) {
        _info.val.visibility = getIntValue();
    }
    else if (!strcmp_P(path, PSTR("timezone"))) {
        _info.val.timezone = getIntValue();
    }
    else if (!strncmp_P(path, PSTR("main."), 5)) {
        if (!strcmp_P(key, PSTR("temp"))) {
            _info.val.temperature = getFloatValue();
        }
        else if (!strcmp_P(key, PSTR("temp_min"))) {
            _info.val.temperature_min = getFloatValue();
        }
        else if (!strcmp_P(key, PSTR("temp_max"))) {
            _info.val.temperature_max = getFloatValue();
        }
        else if (!strcmp_P(key, PSTR("pressure"))) {
            _info.val.pressure = (uint16_t)getIntValue();
        }
        else if (!strcmp_P(key, PSTR("humidity"))) {
            _info.val.humidity = (uint8_t)getIntValue();
        }
    }
    else if (!strncmp_P(path, PSTR("rain."), 5)) {
        _info.rain_mm[keyStr] = getFloatValue();
    }
    else if (!strncmp_P(path, PSTR("weather[]."), 10)) {
        auto &item = _info.weather.back();
        if (!strcmp_P(key, PSTR("main"))) {
            item.main = getValue();
        }
        else if (!strcmp_P(key, PSTR("description"))) {
            item.descr = getValue();
        }
        else if (!strcmp_P(key, PSTR("icon"))) {
            item.icon = getValue();
        }
        else if (!strcmp_P(key, PSTR("id"))) {
            item.id = (uint16_t)getIntValue();;
        }
    }
    else if (!strncmp_P(path, PSTR("sys."), 4)) {
        if (!strcmp_P(key, PSTR("country"))) {
            _info.country = getValue();
        }
        else if (!strcmp_P(key, PSTR("sunrise"))) {
            _info.val.sunrise = getIntValue();
        }
        else if (!strcmp_P(key, PSTR("sunset"))) {
            _info.val.sunset = getIntValue();
        }
    }
    else if (!strncmp_P(path, PSTR("wind."), 5)) {
        if (!strcmp_P(key, PSTR("speed"))) {
            _info.val.wind_speed = getFloatValue();
        }
        else if (!strcmp_P(key, PSTR("deg"))) {
            _info.val.wind_deg = (uint16_t)getIntValue();
        }
    }

    return true;
}

bool OpenWeatherInfoJsonReader::recoverableError(JsonErrorEnum_t errorType) {
    return true;
}
