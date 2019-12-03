/**
* Author: sascha_lammers@gmx.de
*/

#include "OpenWeatherForecastJsonReader.h"

OpenWeatherForecastJsonReader::OpenWeatherForecastJsonReader(Stream * stream, OpenWeatherMapAPI::WeatherForecast & forecast) : JsonBaseReader(stream), _forecast(forecast) {
}

OpenWeatherForecastJsonReader::OpenWeatherForecastJsonReader(OpenWeatherMapAPI::WeatherForecast & forecast) : OpenWeatherForecastJsonReader(nullptr, forecast) {
}

bool OpenWeatherForecastJsonReader::processElement() {

    auto keyStr = getKey();
    auto key = keyStr.c_str();
    auto pathStr = getPath();
    auto path = pathStr.c_str();
    int16_t index = _stack.size() ? _stack.back().arrayIndex : -1;

    Serial.printf("key %s value %s type %s path %s index %d\n", key, getValue().c_str(), jsonType2String(getType()).c_str(), getPath().c_str(), index);

    //if (!strcmp_P(path, "name")) {
    //    _info.location = getValue();
    //}
    //else if (!strcmp_P(path, "visibility")) {
    //    _info.val.visibility = getValue().toInt();
    //}
    //else if (!strcmp_P(path, "timezone")) {
    //    _info.val.timezone = getValue().toInt();
    //}
    //else if (!strncmp_P(path, "main.", 5)) {
    //    if (!strcmp_P(key, "temp")) {
    //        _info.val.temperature = getValue().toFloat();
    //    }
    //    else if (!strcmp_P(key, "temp_min")) {
    //        _info.val.temperature_min = getValue().toFloat();
    //    }
    //    else if (!strcmp_P(key, "temp_max")) {
    //        _info.val.temperature_max = getValue().toFloat();
    //    }
    //    else if (!strcmp_P(key, "pressure")) {
    //        _info.val.pressure = getValue().toFloat();
    //    }
    //    else if (!strcmp_P(key, "humidity")) {
    //        _info.val.humidity = getValue().toFloat();
    //    }
    //}
    //else if (!strncmp_P(path, "rain.", 5)) {
    //    _info.rain_mm[keyStr] = getValue().toFloat();
    //}
    //else if (!strncmp_P(path, "weather[", 8) && index >= 0) {
    //    while((int16_t)_info.weather.size() <= index) {
    //        _info.weather.emplace_back(OpenWeatherMapAPI::WeatherInfo::Weather_t());
    //    }
    //    auto &item = _info.weather.at(index);
    //    if (!strcmp_P(key, "main")) {
    //        item.main = getValue();
    //    }
    //    else if (!strcmp_P(key, "description")) {
    //        item.descr = getValue();
    //    }
    //    else if (!strcmp_P(key, "icon")) {
    //        item.icon = getValue();
    //    }
    //    else if (!strcmp_P(key, "id")) {
    //        item.id = (uint16_t)getValue().toInt();
    //    }
    //}
    //else if (!strncmp_P(path, "sys.", 4)) {
    //    if (!strcmp_P(key, "country")) {
    //        _info.country = getValue();
    //    }
    //    else if (!strcmp_P(key, "sunrise")) {
    //        _info.val.sunrise = getValue().toInt();
    //    }
    //    else if (!strcmp_P(key, "sunset")) {
    //        _info.val.sunset = getValue().toInt();
    //    }
    //}
    //else if (!strncmp_P(path, "wind.", 5)) {
    //    if (!strcmp_P(key, "speed")) {
    //        _info.val.wind_speed = getValue().toFloat();
    //    }
    //    else if (!strcmp_P(key, "deg")) {
    //        _info.val.wind_deg = (uint16_t)getValue().toInt();
    //    }
    //}

    return true;
}

bool OpenWeatherForecastJsonReader::recoverableError(JsonErrorEnum_t errorType) {
    return true;
}
