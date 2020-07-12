/**
* Author: sascha_lammers@gmx.de
*/

#include "OpenWeatherForecastJsonReader.h"
#include "../src/generated/FlashStringGeneratorAuto.h"

OpenWeatherForecastJsonReader::OpenWeatherForecastJsonReader(Stream * stream, OpenWeatherMapAPI::WeatherForecast & forecast) : JsonBaseReader(stream), _forecast(forecast) {
}

OpenWeatherForecastJsonReader::OpenWeatherForecastJsonReader(OpenWeatherMapAPI::WeatherForecast & forecast) : OpenWeatherForecastJsonReader(nullptr, forecast) {
}

bool OpenWeatherForecastJsonReader::beginObject(bool isArray)
{
    auto pathStr = getObjectPath(false);
    auto path = pathStr.c_str();
    //Serial.printf("begin path %s array=%u\n", path, isArray);

    if (!strcmp_P(path, PSTR("list[]"))) {
        _item = OpenWeatherMapAPI::Forecast_t();
        _itemKey = String();
    }
    else if (!strcmp_P(path, PSTR("list[].weather[]"))) {
        _item.weather.emplace_back();
    }

    return true;
}

bool OpenWeatherForecastJsonReader::endObject()
{
    auto pathStr = getObjectPath(false);
    auto path = pathStr.c_str();
    //Serial.printf("end path %s\n", path);

    if (!strcmp_P(path, PSTR("list[]"))) {
        _forecast.forecast.emplace(_itemKey, std::move(_item));
    }
    //if (getLevel() == 1) {
    //    _forecast.updateKeys();
    //}
    return true;
}

bool OpenWeatherForecastJsonReader::processElement()
{
    auto keyStr = getKey();
    auto key = keyStr.c_str();
    auto pathStr = getPath(false);
    auto path = pathStr.c_str();

    //Serial.printf("key %s value %s type %s path %s index %d\n", key, getValue().c_str(), jsonType2String(getType()).c_str(), path, getObjectIndex());

    if (!strncmp_P(path, PSTR("city."), 5)) {
        if (!strcmp_P(key, PSTR("name"))) {
            _forecast.city = getValue();
        }
        else if (!strcmp_P(key, PSTR("country"))) {
            _forecast.country = getValue();
        }
        else if (!strcmp_P(key, PSTR("timezone"))) {
            _forecast.val.timezone = getValue().toInt();
        }
    }
    else if (!strncmp_P(path, PSTR("list[]."), 7)) {
        if (!strncmp_P(path + 7, PSTR("weather[]."), 10)) {
            auto &item = _item.weather.back();
            if (!strcmp_P(key, PSTR("main"))) {
                item.main = getValue();
            }
            else if (!strcmp_P(key, PSTR("description"))) {
                item.descr = getValue();
            }
            else if (!strcmp_P(key, PSTR("icon"))) {
                item.icon = getValue();
            }
            else if (!strcmp_P(key, SPGM(id))) {
                item.id = (uint16_t)getIntValue();;
            }
        }
        else if (!strncmp_P(path + 7, PSTR("main."), 5)) {
            if (!strcmp_P(key, PSTR("temp"))) {
                _item.val.temperature = getFloatValue();
            }
            else if (!strcmp_P(key, PSTR("temp_min"))) {
                _item.val.temperature_min = getFloatValue();
            }
            else if (!strcmp_P(key, PSTR("temp_max"))) {
                _item.val.temperature_max = getFloatValue();
            }
            else if (!strcmp_P(key, PSTR("pressure"))) {
                _item.val.pressure = (uint16_t)getIntValue();
            }
            else if (!strcmp_P(key, PSTR("humidity"))) {
                _item.val.humidity = (uint8_t)getIntValue();
            }

        }
        else if (!strcmp_P(key, PSTR("dt"))) {
            _item.val.time = getIntValue();
        }
        else if (!strcmp_P(key, PSTR("dt_txt"))) {
            _itemKey = getValue();
        }
    }


    /*
    list": [
    {
        "dt": 1575460800,
            "main": {
            "temp": 280.51,
                "temp_min": 276.37,
                "temp_max": 280.51,
                "": 1010,
                "sea_level": 1010,
                "grnd_level": 913,
                "": 99,
                "temp_kf": 4.14
        },
            "weather": [
            {
                "id": 501,
                    "main": "Rain",
                    "description": "moderate rain",
                    "icon": "10n"
            }
            ],
                "clouds": {
                "all": 100
            },
                "wind": {
                    "speed": 2.19,
                        "deg": 181
                },
                    "rain": {
                        "3h": 5.31
                    },
                        "sys": {
                            "pod": "n"
                        },
                            "dt_txt": "2019-12-04 12:00:00"
    },*/

    return true;
}

bool OpenWeatherForecastJsonReader::recoverableError(JsonErrorEnum_t errorType) {
    return true;
}
