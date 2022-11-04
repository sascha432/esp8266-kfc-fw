/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <JsonBaseReader.h>
#include <StreamString.h>
#include <map>

#ifndef DEBUG_OPENWEATHERMAPAPI
#    define DEBUG_OPENWEATHERMAPAPI 0
#endif

#if DEBUG_OPENWEATHERMAPAPI
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

class OpenWeatherMapAPI {
public:
    class Weather_t {
    public:
        Weather_t() : id(0) {
        }
        String main;
        String descr;
        String icon;
        uint16_t id;
    };

    class Forecast_t {
    public:
        Forecast_t() : val({}) {}

        struct {
            time_t time;
            float temperature;
            float temperature_min;
            float temperature_max;
            uint8_t humidity;
            uint16_t pressure;
        } val;
        std::vector<OpenWeatherMapAPI::Weather_t> weather;
    };

    class WeatherInfo {
    public:

        WeatherInfo() : val({}) {}

        time_t getSunRiseAsGMT() const;
        time_t getSunSetAsGMT() const;
        void dump(Print &output) const;

        bool hasData() const
        {
            return hasError() == false && location.length() && weather.size();
        }

        void clear()
        {
            *this = WeatherInfo();
        }

        bool hasError() const {
            return val.error;
        }

        const String &getError() const {
            return country;
        }

        void setError(const String &message) {
            clear();
            country = message;
            val.error = true;
        }

    public:
        String location;
        String country;
        struct {
            int32_t timezone;
            float temperature;
            float temperature_min;
            float temperature_max;
            uint8_t humidity;
            uint16_t pressure;
            time_t sunrise;
            time_t sunset;
            float wind_speed;
            uint16_t wind_deg;
            uint32_t visibility;
            bool error;
        } val;
        std::vector<OpenWeatherMapAPI::Weather_t> weather;
        std::map<String, float> rain_mm;
    };

    class WeatherForecast {
    public:
        WeatherForecast() : val({}) {};

    public:
        //void updateKeys();
        void dump(Print &output) const;

        bool hasData() const
        {
            return hasError() == false && forecast.size() && city.length();
        }

        void clear()
        {
            *this = WeatherForecast();
        }

        bool hasError() const {
            return val.error;
        }

        const String &getError() const {
            return country;
        }

        void setError(const String &message) {
            clear();
            country = message;
            val.error = true;
        }

    public:
        String city;
        String country;
        struct {
            int32_t timezone;
            bool error;
        } val;
        std::map<String, OpenWeatherMapAPI::Forecast_t> forecast;
    };

    OpenWeatherMapAPI();
    OpenWeatherMapAPI(const String &apiKey);

    void clear();

    void setAPIKey(const String &key);

    void setQuery(const String &query);
    String getApiUrl(const String &apiType) const;
    String getWeatherApiUrl() const;
    String getForecastApiUrl() const;

    bool parseWeatherData(const String &data);
    bool parseWeatherData(Stream &stream);
    bool parseForecastData(const String &data);
    bool parseForecastData(Stream &stream);

    JsonBaseReader *getWeatherInfoParser();
    JsonBaseReader *getWeatherForecastParser();

    WeatherInfo &getWeatherInfo();
    WeatherForecast &getWeatherForecast();

    void dump(Print &output) const;

    static float kelvinToC(float temp);
    static float kelvinToF(float temp);
    static float kmToMiles(float km);

private:
    String _apiQuery;
    String _apiKey;
    WeatherInfo _info;
    WeatherForecast _forecast;
};

#if DEBUG_OPENWEATHERMAPAPI
#    include "debug_helper_disable.h"
#endif
