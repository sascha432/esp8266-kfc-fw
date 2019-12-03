/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_WEATHER_STATION

#include "weather_station.h"
#include "moon_phase.h"
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <EventTimer.h>
#include <Timezone.h>
#include <MicrosTimer.h>
#include <HeapStream.h>
#include <ProgmemStream.h>

#include <serial_handler.h>
#include "kfc_fw_config.h"

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// DejaVu_Sans_Bold_20
// DejaVu_Sans_Bold_12
// DejaVu_Sans_Bold_11
// DejaVu_Sans_10
// Dialog_bold_10
// DialogInput_plain_9
#include "fonts.h"

// const GFXfont moon_phases12pt7b
// const GFXfont moon_phases14pt7b
// const GFXfont moon_phases16pt7b
// const GFXfont moon_phases18pt7b
// const GFXfont moon_phases20pt7b
// const GFXfont moon_phases24pt7b
// const GFXfont moon_phases28pt7b
// const GFXfont moon_phases32pt7b
#include "moon.h"

// https://github.com/ThingPulse/esp8266-weather-station-color/blob/master/weathericons.h
#include "weather_icons.h"

// sunrise 7x9px
const unsigned char icon_sunrise [] PROGMEM = {
    0b11111110,
    0b11101110,
    0b11000110,
    0b10000010,
    0b11101110,
    0b11101110,
    0b11101110,
    0b11111110,
    0b00000000,
};
// sunset 7x9px
const unsigned char icon_sunset [] PROGMEM = {
    0b11101110,
    0b11101110,
    0b11101110,
    0b11101110,
    0b10000010,
    0b11000110,
    0b11101110,
    0b11111110,
    0b00000000,
};

#if TFT_WIDTH == 128 && TFT_HEIGHT == 160

#define HAVE_DEGREE_SYMBOL              0
#define DEGREE_STR                      " "

#define COLORS_BACKGROUND               ST77XX_BLACK
#define COLORS_DEFAULT_TEXT             ST77XX_WHITE
#define COLORS_DATE                     ST77XX_WHITE
#define COLORS_TIME                     ST77XX_WHITE
#define COLORS_TIMEZONE                 ST77XX_YELLOW
#define COLORS_CITY                     ST77XX_CYAN
#define COLORS_CURRENT_WEATHER          ST77XX_YELLOW
#define COLORS_TEMPERATURE              ST77XX_WHITE
#define COLORS_WEATHER_DESCR            ST77XX_YELLOW
#define COLORS_SUN_AND_MOON             ST77XX_YELLOW
#define COLORS_SUN_RISE_SET             ST77XX_WHITE
#define COLORS_MOON_PHASE               ST77XX_WHITE

#define FONTS_DEFAULT_SMALL             &DejaVu_Sans_10
#define FONTS_DEFAULT_MEDIUM            &Dialog_bold_10
#define FONTS_DEFAULT_BIG               &DejaVu_Sans_Bold_20
#define FONTS_DATE                      &Dialog_bold_10
#define FONTS_TIME                      &DejaVu_Sans_Bold_20
#define FONTS_TIMEZONE                  &DejaVu_Sans_10
#define FONTS_TEMPERATURE               &DejaVu_Sans_Bold_20
#define FONTS_CITY                      &DialogInput_plain_9
#define FONTS_WEATHER_DESCR             &DialogInput_plain_9
#define FONTS_SUN_AND_MOON              &DejaVu_Sans_10
#define FONTS_MOON_PHASE                &moon_phases14pt7b
#define FONTS_MOON_PHASE_UPPERCASE      true

// time

#define X_POSITION_DATE                 TFT_WIDTH / 2
#define Y_POSITION_DATE                 5
#define H_POSITION_DATE                 CENTER

#define X_POSITION_TIME                 TFT_WIDTH / 2
#define Y_POSITION_TIME                 17
#define H_POSITION_TIME                 CENTER

#define X_POSITION_TIMEZONE             TFT_WIDTH / 2
#define Y_POSITION_TIMEZONE             35
#define H_POSITION_TIMEZONE             CENTER

// weather

#define X_POSITION_WEATHER_ICON         2
#define Y_POSITION_WEATHER_ICON         0

#define X_POSITION_CITY                 (TFT_WIDTH - 2)
#define Y_POSITION_CITY                 3
#define H_POSITION_CITY                 RIGHT

#define X_POSITION_TEMPERATURE          X_POSITION_CITY
#define Y_POSITION_TEMPERATURE          17
#define H_POSITION_TEMPERATURE          RIGHT

#define X_POSITION_WEATHER_DESCR        X_POSITION_CITY
#define Y_POSITION_WEATHER_DESCR        35
#define H_POSITION_WEATHER_DESCR        RIGHT

// sun and moon

#define X_POSITION_SUN_TITLE            2
#define Y_POSITION_SUN_TITLE            0
#define H_POSITION_SUN_TITLE            LEFT

#define X_POSITION_MOON_PHASE_NAME      (TFT_WIDTH - X_POSITION_SUN_TITLE)
#define Y_POSITION_MOON_PHASE_NAME      Y_POSITION_SUN_TITLE
#define H_POSITION_MOON_PHASE_NAME      RIGHT

#define X_POSITION_MOON_PHASE_DAYS      X_POSITION_MOON_PHASE_NAME
#define Y_POSITION_MOON_PHASE_DAYS      12
#define H_POSITION_MOON_PHASE_DAYS      H_POSITION_MOON_PHASE_NAME

#define X_POSITION_SUN_RISE_ICON        4
#define Y_POSITION_SUN_RISE_ICON        10

#define X_POSITION_SUN_SET_ICON         X_POSITION_SUN_RISE_ICON
#define Y_POSITION_SUN_SET_ICON         21

#define X_POSITION_SUN_RISE             (TFT_WIDTH / 3 + 2)
#define Y_POSITION_SUN_RISE             12
#define H_POSITION_SUN_RISE             RIGHT

#define X_POSITION_SUN_SET              X_POSITION_SUN_RISE
#define Y_POSITION_SUN_SET              23
#define H_POSITION_SUN_SET              H_POSITION_SUN_RISE

#define X_POSITION_MOON_PHASE           (TFT_WIDTH / 2)
#define Y_POSITION_MOON_PHASE           12
#define H_POSITION_MOON_PHASE           LEFT


#define Y_START_POSITION_TIME           0
#define Y_END_POSITION_TIME             (45 + Y_START_POSITION_TIME)

#define Y_START_POSITION_WEATHER        (Y_END_POSITION_TIME + 2)
#define Y_END_POSITION_WEATHER          (70 + Y_START_POSITION_WEATHER)

#define Y_START_POSITION_SUN_MOON       (Y_END_POSITION_WEATHER + 2)
#define Y_END_POSITION_SUN_MOON         (TFT_HEIGHT - 1)

// 128x70 = 17920 byte RAM
#define CANVAS_MAX_WIDTH                TFT_WIDTH
#define CANVAS_MAX_HEIGHT               70

#else

#error No theme available for TFT dimensions

#endif

static WeatherStationPlugin plugin;

WeatherStationPlugin::WeatherStationPlugin() :
    _tft(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST),
    _currentScreen(0),
    _updateTimer(0),
    _lastTime(0),
    _timeFormat24h(true),
    _isMetric(true),
    _backlightLevel(1023),
    _pollInterval(0),
    _pollTimer(0),
    _weatherError(F("No data available")),
    _httpClient(nullptr)
{
#if DEBUG_IOT_WEATHER_STATION
    _debugDisplayCanvasBorder = false;
#endif
    pinMode(TFT_PIN_LED, OUTPUT);
    digitalWrite(TFT_PIN_LED, LOW);

    register_plugin(this);
}

PGM_P WeatherStationPlugin::getName() const {
    return PSTR("weather_station");
}

void WeatherStationPlugin::setup(PluginSetupMode_t mode) {

    auto cfg = config._H_GET(Config().weather_station.config);
    _isMetric = cfg.is_metric;
    _timeFormat24h = cfg.time_format_24h;
    _pollInterval = cfg.weather_poll_interval * 60000UL;
    _backlightLevel = std::min(1024 * cfg.backlight_level / 100, 1023);

    _tft.initR(INITR_BLACKTAB);
    _tft.fillScreen(0);
    _tft.setRotation(0);

    _weatherApi.setAPIKey(config._H_STR(Config().weather_station.openweather_api_key));
    _weatherApi.setQuery(config._H_STR(Config().weather_station.openweather_api_query));
    _getWeatherInfo([this](bool status) {
        _draw();
    });

    _draw();
    _fadeBacklight(0, _backlightLevel);

    LoopFunctions::add(loop);
    SerialHandler::getInstance().addHandler(serialHandler, SerialHandler::RECEIVE);
}

void WeatherStationPlugin::reconfigure(PGM_P source) {
}

bool WeatherStationPlugin::hasStatus() const {
    return true;
}

const String WeatherStationPlugin::getStatus() {
    _debug_printf_P(PSTR("WeatherStationPlugin::getStatus()\n"));
    PrintHtmlEntitiesString str;
    str.printf_P(PSTR("WeatherStationPlugin"));
    return str;
}

void WeatherStationPlugin::loop() {
    plugin._loop();
}

void WeatherStationPlugin::serialHandler(uint8_t type, const uint8_t *buffer, size_t len) {
    plugin._serialHandler(buffer, len);
}

void WeatherStationPlugin::_httpRequest(const String &url, uint16_t timeout, JsonBaseReader *jsonReader, Callback_t finishedCallback) {

	if (_httpClient != nullptr) {
        debug_println(F("WeatherStationPlugin::_httpRequest() _httpClient not null, cannot create request"));
        return;
	}

	_httpClient = new asyncHTTPrequest();

    auto onData = [jsonReader](void *ptr, asyncHTTPrequest *request, size_t available) {

        _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(): onData() available=%u\n"), available);

        uint8_t buffer[64];
        size_t len;
        HeapStream stream(buffer);
        jsonReader->setStream(&stream);
        while((len = request->responseRead(buffer, sizeof(buffer))) > 0) {
            stream.setLength(len);
            if (!jsonReader->parseStream()) {
                _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(): onRead(): parseStream() = false\n"));
                request->abort();
                request->onData(nullptr);
                break;
            }
        }
    };

    _httpClient->onData(onData);

	_httpClient->onReadyStateChange([this, url, finishedCallback, jsonReader, onData](void *, asyncHTTPrequest *request, int readyState) {
        _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(): onReadyStateChange() readyState=%u\n"), readyState);
		if (readyState == 4) {
            bool status;
			int httpCode;

             request->onData(nullptr); // disable callback, if any data is left, onPoll() will call it again
			if ((httpCode = request->responseHTTPcode()) == 200) {

                // read rest of the data that was not processed by onData()
                if (request->available()) {
                    onData(nullptr, request, request->available());
                }

                _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(), url=%s, response OK, available = %u\n"), url.c_str(), request->available());
                status = true;

			} else {

                String message;
                int code = 0;

                // read response
                uint8_t buffer[64];
                HeapStream stream(buffer);

                JsonCallbackReader reader(stream, [&message, &code](const String &keyStr, const String &valueStr, size_t partialLength, JsonBaseReader &json) {
                    auto key = keyStr.c_str();
                    if (!strcmp_P(key, PSTR("cod"))) {
                        code = valueStr.toInt();
                    }
                    else if (!strcmp_P(key, PSTR("message"))) {
                        message = valueStr;
                    }
                    return true;
                });
                reader.initParser();

                size_t len;
                while((len = request->responseRead(buffer, sizeof(buffer))) > 0) {
                    stream.setLength(len);
                    if (!reader.parseStream()) {
                        break;
                    }
                }

                // _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(), http code=%d, code=%d, message=%s, url=%s\n"), httpCode, code, message.c_str(), url.c_str());
                _weatherError = F("Failed to load data");
                status = false;

                Logger_error(F("Weather Station: HTTP request failed with HTTP code = %d, API code = %d, message = %s, url = %s"), httpCode, code, message.c_str(), url.c_str());
			}

            // delete object with a delay and inside the main loop, otherwise the program crashes in random places
            auto tmp = _httpClient;
            _httpClient = nullptr;
            delete jsonReader;

            Scheduler.addTimer(100, false, [tmp, finishedCallback, status](EventScheduler::TimerPtr timer) {
                _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest(), delete _httpClient + finishedCallback\n"));
                delete tmp;
                finishedCallback(status);
            });
		}
	});
	_httpClient->setTimeout(timeout);

    _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest() HTTP request=%s\n"), url.c_str());

	if (!_httpClient->open("GET", url.c_str()) || !_httpClient->send()) {
        delete _httpClient;
        delete jsonReader;
        _httpClient = nullptr;

        _debug_printf_P(PSTR("WeatherStationPlugin::_httpRequest() client error, url=%s\n"), url.c_str());
        _weatherError = F("Client error");
        finishedCallback(false);

        Logger_error(F("Weather Station: HTTP request failed, %s, url = %s"), _weatherError.c_str(), url.c_str());
    }
}


void WeatherStationPlugin::_getWeatherInfo(Callback_t finishedCallback) {

#if DEBUG_IOT_WEATHER_STATION
    auto prev = finishedCallback;
    finishedCallback = [this, prev](bool status) {
        _weatherApi.dump(DebugSerial);
        prev(status);
    };

    if (_weatherError.indexOf("avail") != -1) { // load dummy data
        _weatherError = F("Dummy data");
        PGM_P data = PSTR("{\"coord\":{\"lon\":-123.07,\"lat\":49.32},\"weather\":[{\"id\":500,\"main\":\"Rain\",\"description\":\"light rain\",\"icon\":\"10n\"},{\"id\":701,\"main\":\"Mist\",\"description\":\"mist\",\"icon\":\"50n\"}],\"base\":\"stations\",\"main\":{\"temp\":277.55,\"pressure\":1021,\"humidity\":100,\"temp_min\":275.37,\"temp_max\":279.26},\"visibility\":8047,\"wind\":{\"speed\":1.19,\"deg\":165},\"rain\":{\"1h\":0.93},\"clouds\":{\"all\":90},\"dt\":1575357173,\"sys\":{\"type\":1,\"id\":5232,\"country\":\"CA\",\"sunrise\":1575301656,\"sunset\":1575332168},\"timezone\":-28800,\"id\":6090785,\"name\":\"North Vancouver\",\"cod\":200}");
        ProgmemStream stream(data, strlen(data));
        _weatherApi.clear();
        if (!_weatherApi.parseWeatherData(stream)) {
            _weatherError = F("Invalid data");
        }
        finishedCallback(true);
        return;
    }

#endif

    _httpRequest(_weatherApi.getWeatherApiUrl(), std::min(config._H_GET(Config().weather_station.config).api_timeout, (uint16_t)10), _weatherApi.getWeatherInfoParser(), finishedCallback);
}

void WeatherStationPlugin::_fadeBacklight(uint16_t fromLevel, uint16_t toLevel, int8_t step) {
    int8_t direction = fromLevel > toLevel ? -step : step;
    analogWrite(TFT_PIN_LED, fromLevel);

    if (fromLevel != toLevel) {
        Scheduler.addTimer(10, true, [fromLevel, toLevel, direction, step](EventScheduler::TimerPtr timer) mutable {
            if (abs(toLevel - fromLevel) > step) {
                fromLevel += direction;
            } else {
                fromLevel = toLevel;
                timer->detach();
            }
            analogWrite(TFT_PIN_LED, fromLevel);
        }, EventScheduler::PRIO_HIGH);
    }
}

void WeatherStationPlugin::_loop() {
    time_t _time = time(nullptr);

    if (_pollInterval && is_millis_diff_greater(_pollTimer, _pollInterval)) {
        _pollTimer = millis();
        _debug_println(F("WeatherStationPlugin::_loop(): poll interval"));
        //TODO

    }
    else if (is_millis_diff_greater(_updateTimer, 60000)) { // update once per minute
        _draw();
        _updateTimer = millis();
    }
    else if (_currentScreen == 0 && _lastTime != _time) {
        if (_time - _lastTime > 10) {   // time jumped, redraw everything
            _draw();
        }
        else {
            auto tmp = GFXcanvas16(CANVAS_MAX_WIDTH, Y_END_POSITION_TIME - Y_START_POSITION_TIME);
            _canvas = &tmp;
            _drawTime();
            _canvas = nullptr;
        }
    }
}

void WeatherStationPlugin::_serialHandler(const uint8_t *buffer, size_t len) {
    const uint8_t numScreens = 1;
    auto ptr = buffer;
    while(len--) {
        switch(*ptr++) {
            case '+': // next screen
                _currentScreen++;
                _currentScreen %= numScreens;
                _draw();
                break;
            case '-': // prev screen
                _currentScreen += numScreens;
                _currentScreen--;
                _currentScreen %= numScreens;
                _draw();
                break;
            case 't': // switch 12/24h time format
                _timeFormat24h = !_timeFormat24h;
                _draw();
                break;
            case 'm': // switch metric/imperial
                _isMetric = !_isMetric;
                _draw();
                break;
            case '0': // turn backlight off
                _fadeBacklight(_backlightLevel, 0);
                break;
            case '1': // turn backlight on
                _fadeBacklight(0, _backlightLevel);
                break;
            case 'd': // redraw
                _draw();
                break;
            case 'u': // update weather info
                _getWeatherInfo([this](bool status) {
                    _draw();
                });
                break;
            case 'p': // pause
                LoopFunctions::remove(loop);
                break;
            case 'r': // resume
                LoopFunctions::add(loop);
                break;
            case 'g': // gfx debug
                _debugDisplayCanvasBorder = !_debugDisplayCanvasBorder;
                _draw();
                break;
        }
    }
}

void WeatherStationPlugin::_drawTextAligned(Adafruit_GFX &gfx, int16_t x, int16_t y, const String &text, TextAlignEnum_t align, TextVAlignEnum_t valign, Position_t *pos) {

    _drawTextAligned(gfx, x, y, text.c_str(), align, valign, pos);
}

void WeatherStationPlugin::_drawTextAligned(Adafruit_GFX &gfx, int16_t x, int16_t y, const char *text, TextAlignEnum_t align, TextVAlignEnum_t valign, Position_t *pos) {
    int16_t x1, y1;
    uint16_t w, h;

    gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    y -= y1;

    switch(valign) {
        case BOTTOM:
            y -= h;
            break;
        case MIDDLE:
            y -= h / 2;
            break;
        case TOP:
        default:
            break;
    }
    switch(align) {
        case RIGHT:
            x -= w;
            break;
        case CENTER:
            x -= w / 2;
            break;
        case LEFT:
        default:
            break;
    }

    // _debug_printf_P(PSTR("WeatherStationPlugin::_drawTextAligned(): x=%d %d, y=%d %d, w=%u, h=%u\n"), x, x1, y, y1, w, h);

    gfx.setCursor(x, y);
    gfx.print(text);

    if (pos) {
        pos->x = x;
        pos->y = y + y1;
        pos->w = w;
        pos->h = h;
    }

}

void WeatherStationPlugin::_drawBitmap(Adafruit_GFX &gfx, int16_t x, int16_t y, PGM_P bmp, const uint16_t *palette, Dimensions_t *dim) {

    uint16_t width = 0, height = 0;

    auto dataPtr = bmp + 1;
    if (pgm_read_byte(dataPtr++) == 2) { // only 2 bit depth supported
        width = (pgm_read_byte(dataPtr++) >> 8);
        width |= pgm_read_byte(dataPtr++);
        height = (pgm_read_byte(dataPtr++) >> 8);
        height |= pgm_read_byte(dataPtr++);

        const uint8_t numColors = 4;
        uint16_t _palette[numColors];
        memcpy_P(_palette, palette, sizeof(_palette));

        // _debug_printf_P(PSTR("_drawBitmap(): %u (%u) x %u x %u\n"), width, ((width + 7) & ~7), height, pgm_read_byte(bmp + 1));

        width += x; // set to x end position
        uint8_t ofs = (x & 3); // first bit relative to x1, indicator for reading a new byte
        uint8_t byte = 0;
        while(height--) {
            for(uint16_t x1 = x; x1 < width; x1++) {
                if ((x1 & 3) == ofs) {
                    byte = pgm_read_byte(dataPtr++);
                    byte = (byte >> 6) | (((byte >> 4) & 0x3) << 2) | (((byte >> 2) & 0x3) << 4) | (((byte) & 0x3) << 6);   // invert pixel order
                }
                else {
                    byte >>= 2;
                }
                gfx.drawPixel(x1, y, _palette[(byte & 0x3) % numColors]);
            }
            y++;
        }
    }

    if (dim) {
        dim->w = width;
        dim->h = height;
    }
}

void WeatherStationPlugin::_drawRGBBitmap(int16_t x, int16_t y, uint16_t *pcolors, int16_t w, int16_t h) {
#if DEBUG_IOT_WEATHER_STATION

    static int16_t max_height = 0;
    max_height = std::max(max_height, h);

    _tft.drawRGBBitmap(x, y, pcolors, w, h);

    if (_debugDisplayCanvasBorder) {
        _debug_printf_P(PSTR("WeatherStationPlugin::_drawRGBBitmap(): x=%d, y=%d, w=%u, h=%u (max. %u)\n"), x, y, w, h, max_height);
        uint16_t colors[] = { ST77XX_CYAN, ST77XX_RED, ST77XX_YELLOW, ST77XX_BLUE };
        _tft.drawRect(x, y, w, h, colors[y % 4]);
    }

#else

    _tft.drawRGBBitmap(x, y, pcolors, w, h);

#endif
}

void WeatherStationPlugin::_drawTime() {
    // _debug_printf_P(PSTR("WeatherStationPlugin::_drawTime()\n"));

    char buf[32];
    _lastTime = time(nullptr);
    if (!_canvas->getBuffer()) { // could not allocate memory, retry later
        return;
    }

    struct tm *tm = timezone_localtime(&_lastTime);

    _canvas->fillScreen(COLORS_BACKGROUND);
    _canvas->setFont(FONTS_DATE);
    _canvas->setTextColor(COLORS_DATE);
    timezone_strftime_P(buf, sizeof(buf), PSTR("%a %b %d %Y"), tm);
    _drawTextAligned(*_canvas, X_POSITION_DATE, Y_POSITION_DATE, buf, H_POSITION_DATE);

    _canvas->setFont(FONTS_TIME);
    _canvas->setTextColor(COLORS_TIME);
    if (_timeFormat24h) {
        timezone_strftime_P(buf, sizeof(buf), PSTR("%H:%M:%S"), tm);
    }
    else {
        timezone_strftime_P(buf, sizeof(buf), PSTR("%I:%M:%S"), tm);
    }
    _drawTextAligned(*_canvas, X_POSITION_TIME, Y_POSITION_TIME, buf, H_POSITION_TIME);

    _canvas->setFont(FONTS_TIMEZONE);
    _canvas->setTextColor(COLORS_TIMEZONE);
    if (_timeFormat24h) {
        timezone_strftime_P(buf, sizeof(buf), PSTR("%Z"), tm);
    }
    else {
        timezone_strftime_P(buf, sizeof(buf), PSTR("%p - %Z"), tm);
    }
    _drawTextAligned(*_canvas, X_POSITION_TIMEZONE, Y_POSITION_TIMEZONE, buf, H_POSITION_TIMEZONE);

    _drawRGBBitmap(0, Y_START_POSITION_TIME, _canvas->getBuffer(), TFT_WIDTH, Y_END_POSITION_TIME - Y_START_POSITION_TIME);
}

void WeatherStationPlugin::_drawWeather() {

    _debug_printf_P(PSTR("WeatherStationPlugin::_drawWeather()\n"));

    _canvas->fillScreen(COLORS_BACKGROUND);

    auto &info = _weatherApi.getWeatherInfo();
    if (info.hasData()) {

        // --- location

        static const uint16_t palette[] PROGMEM = { COLORS_BACKGROUND, ST77XX_WHITE, ST77XX_YELLOW, ST77XX_BLUE };
        _drawBitmap(*_canvas, X_POSITION_WEATHER_ICON, Y_POSITION_WEATHER_ICON, getMiniMeteoconIconFromProgmem(info.weather[0].icon), palette);

        // create kind of shadow effect in case the text is drawn over the icon
        _canvas->setFont(FONTS_CITY);
        _canvas->setTextColor(COLORS_BACKGROUND);
        for(int8_t x = -2; x <= 2; x++) {
            for(int8_t y = 0; y < 2; y++) {
                if (!(y == 0 && x == 0)) {
                    _drawTextAligned(*_canvas, X_POSITION_CITY + x, Y_POSITION_CITY + y, info.location, H_POSITION_CITY, TOP);
                }
            }
        }
        _canvas->setTextColor(COLORS_CITY);
        _drawTextAligned(*_canvas, X_POSITION_CITY, Y_POSITION_CITY, info.location, H_POSITION_CITY);

        // --- temperature

        _canvas->setFont(FONTS_TEMPERATURE);
        _canvas->setTextColor(COLORS_TEMPERATURE);
        String unit, temp;
        if (_isMetric) {
            unit = F(DEGREE_STR "C");
            temp = String(OpenWeatherMapAPI::kelvinToC(info.val.temperature), 1) + unit;
        }
        else {
            unit = F(DEGREE_STR "F");
            temp = String(OpenWeatherMapAPI::kelvinToF(info.val.temperature), 1) + unit;
        }

#if HAVE_DEGREE_SYMBOL
        _drawTextAligned(*_canvas, X_POSITION_TEMPERATURE, Y_POSITION_TEMPERATURE, temp, H_POSITION_TEMPERATURE, TOP);
#else
        Position_t pos;
        _drawTextAligned(*_canvas, X_POSITION_TEMPERATURE, Y_POSITION_TEMPERATURE, temp, H_POSITION_TEMPERATURE, TOP, &pos);

        // draw ° before unit
        {
            int16_t x1, y1;
            uint16_t w, h;
            _canvas->getTextBounds(unit, 0, 0, &x1, &y1, &w, &h);
            uint8_t r = h / 5;
            r = std::max(std::min(5, (int)r), 1);
            x1 = pos.x + pos.w - (w * 5 / 6);
            y1 = Y_POSITION_TEMPERATURE + r;
            do {
                _canvas->drawCircle(x1, y1, r--, COLORS_TEMPERATURE);
            } while(r > 1);
        }
#endif

        // --- weather description

        _canvas->setFont(FONTS_WEATHER_DESCR);
        _canvas->setTextColor(COLORS_WEATHER_DESCR);

        String tmp = info.weather[0].descr;
        if (tmp.length() > 10) {
            auto idx = tmp.indexOf(' '); // wrap after first word and align to the right
            if (idx != -1) {
                _drawTextAligned(*_canvas, X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR, tmp.substring(0, idx), H_POSITION_WEATHER_DESCR, TOP, &pos);
                _drawTextAligned(*_canvas, X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR + pos.h + 2, tmp.substring(idx + 1), H_POSITION_WEATHER_DESCR, TOP);
                tmp = String();
            }
        }
        if (tmp.length()) {
            _drawTextAligned(*_canvas, X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR, tmp, H_POSITION_WEATHER_DESCR);
        }

    }
    else {
        _canvas->setFont(FONTS_DEFAULT_MEDIUM);
        _canvas->setTextColor(COLORS_DEFAULT_TEXT);
        _drawTextAligned(*_canvas, TFT_WIDTH / 2, (Y_END_POSITION_WEATHER - Y_START_POSITION_WEATHER) / 2, _weatherError, CENTER, MIDDLE);
    }

    _drawRGBBitmap(0, Y_START_POSITION_WEATHER, _canvas->getBuffer(), TFT_WIDTH, Y_END_POSITION_WEATHER - Y_START_POSITION_WEATHER);
}

void WeatherStationPlugin::_drawSunAndMoon() {

    _debug_printf_P(PSTR("WeatherStationPlugin::_drawSunAndMoon()\n"));

    double moonDay;
    uint8_t moonPhase;
    char moonPhaseFont;
    calcMoon(time(nullptr), moonDay, moonPhase, moonPhaseFont, FONTS_MOON_PHASE_UPPERCASE);

    _canvas->fillScreen(COLORS_BACKGROUND);

    _canvas->setFont(FONTS_SUN_AND_MOON);
    _canvas->setTextColor(COLORS_SUN_AND_MOON);
    _drawTextAligned(*_canvas, X_POSITION_SUN_TITLE, Y_POSITION_SUN_TITLE, F("Sun"), H_POSITION_SUN_TITLE);
    _drawTextAligned(*_canvas, X_POSITION_MOON_PHASE_NAME, Y_POSITION_MOON_PHASE_NAME, moonPhaseName(moonPhase), H_POSITION_MOON_PHASE_NAME);
    _drawTextAligned(*_canvas, X_POSITION_MOON_PHASE_DAYS, Y_POSITION_MOON_PHASE_DAYS, String((int)round(moonDay)) + 'd', H_POSITION_MOON_PHASE_NAME);

    auto &info = _weatherApi.getWeatherInfo();
    if (info.hasData()) {
        _canvas->setTextColor(COLORS_SUN_RISE_SET);
        _canvas->drawBitmap(X_POSITION_SUN_RISE_ICON, Y_POSITION_SUN_RISE_ICON, icon_sunrise, 7, 9, COLORS_BACKGROUND, COLORS_SUN_RISE_SET);
        _canvas->drawBitmap(X_POSITION_SUN_SET_ICON, Y_POSITION_SUN_SET_ICON, icon_sunset, 7, 9, COLORS_BACKGROUND, COLORS_SUN_RISE_SET);

        char buf[8];
        PGM_P timeFormat = PSTR("%H:%M");
        time_t time = info.getSunRiseAsGMT();
        struct tm *tm = gmtime(&time);
        strftime_P(buf, sizeof(buf), timeFormat, tm);
        _drawTextAligned(*_canvas, X_POSITION_SUN_SET, Y_POSITION_SUN_RISE, buf, H_POSITION_SUN_RISE);
        time = info.getSunSetAsGMT();
        tm = gmtime(&time);
        strftime_P(buf, sizeof(buf), timeFormat, tm);
        _drawTextAligned(*_canvas, X_POSITION_SUN_SET, Y_POSITION_SUN_SET, buf, H_POSITION_SUN_SET);
    }

    _canvas->setFont(FONTS_MOON_PHASE);
    _canvas->setTextColor(COLORS_MOON_PHASE);
    _drawTextAligned(*_canvas, X_POSITION_MOON_PHASE, Y_POSITION_MOON_PHASE, String(moonPhaseFont), H_POSITION_MOON_PHASE);

    _drawRGBBitmap(0, Y_START_POSITION_SUN_MOON, _canvas->getBuffer(), TFT_WIDTH, Y_END_POSITION_SUN_MOON - Y_START_POSITION_SUN_MOON);
}

void WeatherStationPlugin::_drawScreen0() {
    _drawTime();
    _drawWeather();
    _drawSunAndMoon();
}

void WeatherStationPlugin::_draw() {

    _debug_printf_P(PSTR("WeatherStationPlugin::_draw()\n"));

    auto tmp = GFXcanvas16(CANVAS_MAX_WIDTH, CANVAS_MAX_HEIGHT);
    if (!tmp.getBuffer()) { // could not allocate memory
        return;
    }
    _canvas = &tmp;

    switch(_currentScreen) {
        case 0:
            _drawScreen0();
            break;
    }
    _canvas = nullptr;
}

#endif
