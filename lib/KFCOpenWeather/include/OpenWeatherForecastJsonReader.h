/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <JsonBaseReader.h>
#include "OpenWeatherMapAPI.h"

class OpenWeatherForecastJsonReader : public JsonBaseReader {
public:
    OpenWeatherForecastJsonReader(Stream *stream, OpenWeatherMapAPI::WeatherForecast &forecast);
    OpenWeatherForecastJsonReader(OpenWeatherMapAPI::WeatherForecast &forecast);

    virtual bool processElement();
    virtual bool recoverableError(JsonErrorEnum_t errorType);

private:
    OpenWeatherMapAPI::WeatherForecast &_forecast;
};
