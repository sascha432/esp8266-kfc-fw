/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <JsonBaseReader.h>
#include "OpenWeatherMapAPI.h"

class OpenWeatherInfoJsonReader : public JsonBaseReader {
public:
    OpenWeatherInfoJsonReader(Stream *stream, OpenWeatherMapAPI::WeatherInfo &info);
    OpenWeatherInfoJsonReader(OpenWeatherMapAPI::WeatherInfo &info);

    virtual bool processElement();
    virtual bool recoverableError(JsonErrorEnum_t errorType);

private:
    OpenWeatherMapAPI::WeatherInfo &_info;
};
