/**
 * Author: sascha_lammers@gmx.de
 */

#include "weather_station.h"
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <EventTimer.h>
#include <MicrosTimer.h>
#include <HeapStream.h>
#include <ProgmemStream.h>
#include <serial_handler.h>
#include <WebUISocket.h>
#include <AsyncBitmapStreamResponse.h>
#include <GFXCanvasRLEStream.h>
#include "RestAPI.h"
#include "web_server.h"
#include "progmem_data.h"
#include "./plugins/sensor/sensor.h"
#include "./plugins/sensor/Sensor_BME280.h"
#if IOT_WEATHER_STATION_COMP_RH
#include "./plugins/sensor/EnvComp.h"
#endif
#include "kfc_fw_config.h"

// web accesss for screen capture
//
// screen capture
// http://192.168.0.91/images/screen_capture.bmp
//
//

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(weather_station_webui_id, "weather_station");

static WeatherStationPlugin plugin;

WeatherStationPlugin::WeatherStationPlugin() :
    WSDraw(),
    _updateTimer(0),
    _backlightLevel(1023),
    _pollTimer(0),
    _httpClient(nullptr)
{
#if DEBUG_IOT_WEATHER_STATION
    _debugDisplayCanvasBorder = false;
#endif
    REGISTER_PLUGIN(this);
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    _touchpadDebug = false;
#endif
}

PGM_P WeatherStationPlugin::getName() const
{
    return PSTR("weather");
}

void WeatherStationPlugin::_sendScreenCaptureBMP(AsyncWebServerRequest *request)
{
    // _debug_printf_P(PSTR("WeatherStationPlugin::_sendScreenCapture(): is_authenticated=%u\n"), web_server_is_authenticated(request));

    if (web_server_is_authenticated(request)) {
        //auto response = new AsyncClonedBitmapStreamResponse(plugin.getCanvas().clone());
        debug_printf("AsyncBitmapStreamResponse\n");
        auto response = new AsyncBitmapStreamResponse(plugin.getCanvas());
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache();
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void WeatherStationPlugin::_installWebhooks()
{
    _debug_printf_P(PSTR("WeatherStationPlugin::_installWebhooks(): Installing web handler for screen capture, server=%p\n"), get_web_server_object());
    web_server_add_handler(F("/images/screen_capture.bmp"), _sendScreenCaptureBMP);
}

void WeatherStationPlugin::_readConfig()
{
    _config = WeatherStation::getConfig();
    _backlightLevel = std::min(1024 * _config.backlight_level / 100, 1023);
}


void WeatherStationPlugin::setup(PluginSetupMode_t mode)
{
    _readConfig();
    _init();

    _weatherApi.setAPIKey(WeatherStation::getApiKey());
    _weatherApi.setQuery(WeatherStation::getQueryString());
    _getWeatherInfo([this](bool status) {
        _draw();
    });

    _draw();
    _fadeBacklight(0, _backlightLevel);
    int progressValue = -1;
    web_server_set_update_firmware_callback([this, progressValue](size_t position, size_t size) mutable {
        int progress = position * 100 / size;
        if (progressValue != progress) {
            if (progressValue == -1) {
                _currentScreen = TEXT_CLEAR;
            }
            setText(PrintString(F("Updating\n%d%%"), progress), FONTS_DEFAULT_MEDIUM);
            progressValue = progress;
        }
    });

#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (!_touchpad.begin(MPR121_I2CADDR_DEFAULT, IOT_WEATHER_STATION_MPR121_PIN, &config.initTwoWire())) {
        Logger_error(F("Failed to initialize touch sensor"));
    }
    else {
        _touchpad.getMPR121().setThresholds(_config.touch_threshold, _config.released_threshold);
    }
#endif

#if IOT_WEATHER_STATION_TEMP_COMP
    auto compensationCallback = [this](Sensor_BME280::SensorData_t &sensor) {
        float realTemp;
#if IOT_WEATHER_STATION_TEMP_COMP == 1
        realTemp = sensor.temperature + IOT_WEATHER_STATION_TEMP_COMP_OFS;
#elif IOT_WEATHER_STATION_TEMP_COMP == 2
        realTemp = config.getRTCTemperature()  + IOT_WEATHER_STATION_TEMP_COMP_OFS;
#else
#error TODO
#endif

#if IOT_WEATHER_STATION_COMP_RH
        sensor.humidity = EnvComp::getCompensatedRH(sensor.temperature, sensor.humidity, realTemp);
#endif
        sensor.temperature = realTemp;
    };
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTTSensorSensorType::ENUM::BME280) {
            reinterpret_cast<Sensor_BME280 *>(sensor)->setCompensationCallback(compensationCallback);
        }
    }

#endif

    LoopFunctions::add(loop);

#if IOT_WEATHER_STATION_WS2812_NUM
    WiFiCallbacks::add(WiFiCallbacks::CONNECTED, [this](uint8_t event, void *payload) {
        _fadeStatusLED();
    }, this);

    if (config.isWiFiUp()) {
        _fadeStatusLED();
    }
#endif

    // SerialHandler::getInstance().addHandler(serialHandler, SerialHandler::RECEIVE);

    _installWebhooks();

/*
    WsClient::addClientCallback([this](WsClient::ClientCallbackTypeEnum_t type, WsClient *client) {
        if (type == WsClient::ClientCallbackTypeEnum_t::AUTHENTICATED) {
            // draw sends the screen capture to the web socket, do it for each new client
            Scheduler.addTimer(100, false, [this](EventScheduler::TimerPtr timer) {
                _draw();
            });
        }
    });
    */
}

void WeatherStationPlugin::reconfigure(PGM_P source)
{
    auto oldLevel = _backlightLevel;
    _readConfig();
    _installWebhooks();
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    _touchpad.getMPR121().setThresholds(_config.touch_threshold, _config.released_threshold);
#endif
    _fadeBacklight(oldLevel, _backlightLevel);
}

void WeatherStationPlugin::restart()
{
#if IOT_WEATHER_STATION_WS2812_NUM
    _pixelTimer.remove();
#endif
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    _touchpad.end();
#endif
    _fadeTimer.remove();
    _drawText(F("Rebooting\nDevice"), FONTS_DEFAULT_BIG, COLORS_DEFAULT_TEXT, true);
    LoopFunctions::remove(loop);
    // SerialHandler::getInstance().removeHandler(serialHandler);
}

bool WeatherStationPlugin::hasReconfigureDependecy(PluginComponent *plugin) const
{
    return plugin->nameEquals(FSPGM(http));
}

bool WeatherStationPlugin::hasStatus() const
{
    return true;
}

void WeatherStationPlugin::getStatus(Print &output)
{
    _debug_printf_P(PSTR("WeatherStationPlugin::getStatus()\n"));
    output.printf_P(PSTR("Weather Station Plugin"));
}

void WeatherStationPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    using KFCConfigurationClasses::Plugins;

    //auto cfg = Plugins::WeatherStation::getConfig();

    form.setFormUI(F("Weather Station Configuration"));

    form.add<uint8_t>(F("time_format_24h"), _H_FLAGS_VALUE(MainConfig().plugins.weatherstation.config, time_format_24h))->setFormUI((new FormUI(FormUI::SELECT, F("Time Format")))->setBoolItems(F("24h"), F("12h")));
    form.add<uint8_t>(F("is_metric"), _H_FLAGS_VALUE(MainConfig().plugins.weatherstation.config, is_metric))->setFormUI((new FormUI(FormUI::SELECT, F("Units")))->setBoolItems(F("Metric"), F("Imperial")));

    form.add<uint16_t>(F("weather_poll_interval"), _H_STRUCT_VALUE(MainConfig().plugins.weatherstation.config, weather_poll_interval))->setFormUI((new FormUI(FormUI::TEXT, F("Weather Poll Interval")))->setSuffix(F("minutes")));
    form.add<uint16_t>(F("api_timeout"), _H_STRUCT_VALUE(MainConfig().plugins.weatherstation.config, api_timeout))->setFormUI((new FormUI(FormUI::TEXT, F("API Timeout")))->setSuffix(F("seconds")));

    form.add<uint8_t>(F("backlight_level"), _H_STRUCT_VALUE(MainConfig().plugins.weatherstation.config, backlight_level))->setFormUI((new FormUI(FormUI::TEXT, F("Backlight Level")))->setSuffix(F("&#37;")));
    form.add<uint8_t>(F("touch_threshold"), _H_STRUCT_VALUE(MainConfig().plugins.weatherstation.config, touch_threshold))->setFormUI(new FormUI(FormUI::TEXT, F("Touch Threshold")));
    form.add<uint8_t>(F("released_threshold"), _H_STRUCT_VALUE(MainConfig().plugins.weatherstation.config, released_threshold))->setFormUI(new FormUI(FormUI::TEXT, F("Release Threshold")));

    // form.add<uint8_t>(F("blink_colon"), &clock->blink_colon)->setFormUI(
    //     (new FormUI(FormUI::SELECT, F("Blink Colon")))
    //         ->addItems(String(BlinkColonEnum_t::SOLID), F("Solid"))
    //         ->addItems(String(BlinkColonEnum_t::NORMAL), F("Normal"))
    //         ->addItems(String(BlinkColonEnum_t::FAST), F("Fast"))
    // );
    // form.addValidator(new FormRangeValidator(F("Invalid value"), BlinkColonEnum_t::SOLID, BlinkColonEnum_t::FAST));

    // form.add<int8_t>(F("animation"), &clock->animation)->setFormUI(
    //     (new FormUI(FormUI::SELECT, F("Animation")))
    //         ->addItems(String(AnimationEnum_t::NONE), F("Solid"))
    //         ->addItems(String(AnimationEnum_t::RAINBOW), F("Rainbow"))
    // );
    // form.addValidator(new FormRangeValidator(F("Invalid animation"), AnimationEnum_t::NONE, AnimationEnum_t::FADE));

    // form.add<uint8_t>(F("brightness"), &clock->brightness)
    //     ->setFormUI((new FormUI(FormUI::TEXT, F("Brightness")))->setSuffix(F("0-255")));

    // String str = PrintString(F("#%02X%02X%02X"), clock->solid_color[0], clock->solid_color[1], clock->solid_color[2]);
    // form.add(F("solid_color"), str)
    //     ->setFormUI(new FormUI(FormUI::TEXT, F("Solid Color")));
    // form.addValidator(new FormCallbackValidator([](const String &value, FormField &field) {
    //     auto ptr = value.c_str();
    //     if (*ptr == '#') {
    //         ptr++;
    //     }
    //     auto color = strtol(ptr, nullptr, 16);
    //     auto &colorArr = config._H_W_GET(Config().clock).solid_color;
    //     colorArr[0] = color >> 16;
    //     colorArr[1] = color >> 8;
    //     colorArr[2] = (uint8_t)color;
    //     return true;
    // }));

    form.finalize();

}

void WeatherStationPlugin::loop()
{
    plugin._loop();
}

void WeatherStationPlugin::_init()
{
    pinMode(TFT_PIN_LED, OUTPUT);
    digitalWrite(TFT_PIN_LED, LOW);
    _debug_printf_P(PSTR("tft: cs=%d, dc=%d, rst=%d\n"), TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);
    _tft.initR(INITR_BLACKTAB);
    _tft.fillScreen(0);
    _tft.setRotation(0);
}


WeatherStationPlugin &WeatherStationPlugin::_getInstance()
{
    return plugin;
}

void WeatherStationPlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Weather Station"), false);

    row = &webUI.addRow();
    row->addScreen(FSPGM(weather_station_webui_id), _canvas.width(), _canvas.height());
}


// void WeatherStationPlugin::serialHandler(uint8_t type, const uint8_t *buffer, size_t len)
// {
//     plugin._serialHandler(buffer, len);
// }

void WeatherStationPlugin::getValues(JsonArray &array)
{
    _debug_printf_P(PSTR("WeatherStationPlugin::getValues()\n"));
    LoopFunctions::callOnce([this]() {
        _draw();
    });
}

void WeatherStationPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    _debug_printf_P(PSTR("WeatherStationPlugin::setValue()\n"));
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"
// #include <Adafruit_NeoPixel.h>

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSSET, "WSSET", "<on/off>,<touchpad/timeformat24h/metrics/tft/scroll>", "Enable/disable function");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSBL, "WSBL", "<level=0-1023>", "Set backlight level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSU, "WSU", "<i/f>", "Update weather info/forecast");

void WeatherStationPlugin::atModeHelpGenerator()
{
    auto name = getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WSSET), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WSBL), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WSU), name);
}

bool WeatherStationPlugin::atModeHandler(AtModeArgs &args)
{
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSSET))) {
        if (args.requireArgs(2, 2)) {
            bool state = args.isTrue(0);
            if (args.equalsIgnoreCase(1, F("touchpad"))) {
                _touchpadDebug = state;
                args.printf_P(PSTR("touchpad debug=%u"), state);
            }
            else if (args.equalsIgnoreCase(1, F("timeformat24h"))) {
                _config.time_format_24h = state;
                _draw();
                args.printf_P(PSTR("time format 24h=%u"), state);
            }
            else if (args.equalsIgnoreCase(1, F("metrics"))) {
                _config.is_metric = state;
                _draw();
                args.printf_P(PSTR("metrics=%u"), state);
            }
            else if (args.equalsIgnoreCase(1, F("tft"))) {
                _tft.initR(INITR_BLACKTAB);
                _tft.fillScreen(state ? ST77XX_YELLOW : ST77XX_RED);
                _tft.setRotation(0);
                args.printf_P(PSTR("set color"));
            }
            else if (args.equalsIgnoreCase(1, F("scroll"))) {
                if (!_isScrolling()) {
                    _doScroll();
                    _drawEnvironmentalSensor(_scrollCanvas->getCanvas(), 0);
                    Scheduler.addTimer(20, true, [this](EventScheduler::TimerPtr timer) {
                        _scrollTimer(*this);
                    });
                }
            }
            else if (args.equalsIgnoreCase(1, F("text"))) {
                setText(args.get(0), FONTS_DEFAULT_MEDIUM);
                _currentScreen = TEXT_CLEAR;
            }
            else {
                args.print("Invalid type");
            }
        }
        return true;
    }
#endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSBL))) {
        if (args.requireArgs(1, 1)) {
            uint16_t level = args.toIntMinMax<uint16_t>(0, 0, 1023);
            _fadeBacklight(_backlightLevel, level);
            _backlightLevel = level;
            args.printf_P(PSTR("backlight=%d"), level);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSU))) {
        if (args.equals(0, 'f')) {
            args.print(F("Updating forecast..."));
            _getWeatherForecast([this, args](bool status) mutable {
                args.printf_P(PSTR("status=%u"), status);
                _draw();
            });
        }
        else {
            args.print(F("Updating info..."));
            _getWeatherInfo([this, args](bool status) mutable {
                args.printf_P(PSTR("status=%u"), status);
                _draw();
            });
        }
        return true;
    }
    return false;
}

#endif

void WeatherStationPlugin::_drawEnvironmentalSensor(GFXCanvasCompressed& canvas, uint8_t top)
{
    // _debug_printf_P(PSTR("WSDraw::_drawEnvironmentalSensor()\n"));
    _offsetY = top;

    Sensor_BME280::SensorData_t values = { NAN, NAN, NAN };
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTTSensorSensorType::ENUM::BME280) {
            auto &bme280 = *(reinterpret_cast<Sensor_BME280 *>(sensor));
            values = bme280.readSensor();
            break;
        }
    }

    canvas.setFont(FONTS_WEATHER_DESCR);
    canvas.setTextColor(COLORS_WEATHER_DESCR);
    PrintString str(F("\n\nTemperature %.2f°C\nHumidty %.2f%%\nPressure %.2fhPa"), values.temperature, values.humidity, values.pressure);
    canvas._drawTextAligned(0, _offsetY, str, AdafruitGFXExtension::LEFT);
}

void WeatherStationPlugin::_httpRequest(const String &url, int timeout, JsonBaseReader *jsonReader, Callback_t finishedCallback)
{
    auto rest = new ::WeatherStation::RestAPI(url);
    rest->setAutoDelete(true);
    rest->call(jsonReader, std::max(15, timeout), [this, url, finishedCallback](bool status, const String &error) {
        _debug_printf_P(PSTR("status=%u error=%s url=%s\n"), status, error.c_str(), url.c_str());
        if (!status) {
            _weatherError = F("Failed to load data");
        }
        LoopFunctions::callOnce([finishedCallback, status]() {
            finishedCallback(status);
        });
    });
}


void WeatherStationPlugin::_getWeatherInfo(Callback_t finishedCallback)
{
#if DEBUG_IOT_WEATHER_STATION
    auto prev = finishedCallback;
    finishedCallback = [this, prev](bool status) {
        _weatherApi.dump(DebugSerial);
        prev(status);
    };

#if 0
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

#endif

    _httpRequest(_weatherApi.getWeatherApiUrl(), WeatherStation::getConfig().api_timeout, _weatherApi.getWeatherInfoParser(), finishedCallback);
}

void WeatherStationPlugin::_getWeatherForecast(Callback_t finishedCallback)
{
#if DEBUG_IOT_WEATHER_STATION
    auto prev = finishedCallback;
    finishedCallback = [this, prev](bool status) {
        _weatherApi.dump(DebugSerial);
        prev(status);
    };
#endif

    _httpRequest(_weatherApi.getForecastApiUrl(), WeatherStation::getConfig().api_timeout, _weatherApi.getWeatherForecastParser(), finishedCallback);
}

void WeatherStationPlugin::_fadeBacklight(uint16_t fromLevel, uint16_t toLevel, int8_t step)
{
    int8_t direction = fromLevel > toLevel ? -step : step;
    analogWrite(TFT_PIN_LED, fromLevel);

    if (fromLevel != toLevel) {
        _fadeTimer.add(10, true, [fromLevel, toLevel, direction, step](EventScheduler::TimerPtr timer) mutable {
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

void WeatherStationPlugin::_fadeStatusLED()
{
#if IOT_WEATHER_STATION_WS2812_NUM
    uint32_t color = 0x001500;
    int16_t dir = 0x100;
    NeoPixel_fillColor(_pixels, sizeof(_pixels), color);
    NeoPixel_espShow(IOT_WEATHER_STATION_WS2812_PIN, _pixels, sizeof(_pixels), true);
    _pixelTimer.add(50, true, [this, color, dir](EventScheduler::TimerPtr timer) mutable {
        color += dir;
        if (color >= 0x003000) {
            dir = -dir;
            color = 0x003000;
        }
        else if (color == 0) {
            timer->detach();
        }
        NeoPixel_fillColor(_pixels, sizeof(_pixels), color);
        NeoPixel_espShow(IOT_WEATHER_STATION_WS2812_PIN, _pixels, sizeof(_pixels), true);
    });
#endif
}

void WeatherStationPlugin::_loop()
{
    time_t _time = time(nullptr);

#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (_touchpadDebug) {
        SpeedBooster speedBooster;

        const auto &coords = _touchpad.get();
        uint16_t colorTouched = ST77XX_RED;
        uint16_t colorPredict = ST77XX_YELLOW;
        uint16_t colorGrid = ST77XX_WHITE;
        _canvas.fillScreen(COLORS_BACKGROUND);
        for(int yy = 1; yy <= 8; yy++) {
            _canvas.drawLine(10, yy * 10, 8 * 14, yy * 10, yy == coords.y ? colorTouched : (yy == _touchpad._event._predict.y ? colorPredict : colorGrid));
        }
        for(int xx = 1; xx <= 13; xx++) {
            _canvas.drawLine(xx * 8, 10, xx * 8, 8 * 10, xx == coords.x ? colorTouched : (xx == _touchpad._event._predict.x ? colorPredict : colorGrid));
        }

        _displayScreen(0, 0, TFT_WIDTH, 85);
        return;
    }
#endif

    if (_config.weather_poll_interval && is_millis_diff_greater(_pollTimer, _config.getPollIntervalMillis())) {
        _pollTimer = millis();
        _debug_println(F("WeatherStationPlugin::_loop(): poll interval"));
        //TODO

        _getWeatherInfo([this](bool status) {
            _draw();
        });

    }
    else if (is_millis_diff_greater(_updateTimer, 60000)) { // update once per minute
        _draw();
        _updateTimer = millis();
    }
    else if (_currentScreen == TEXT_CLEAR) {
        _draw();
    }
    else if (_currentScreen == MAIN && _lastTime != _time) {
        if (_time - _lastTime > 10) {   // time jumped, redraw everything
            _draw();
        }
        else {
            _updateTime();
        }
    }
}

// void WeatherStationPlugin::_serialHandler(const uint8_t *buffer, size_t len)
// {
//     const uint8_t numScreens = 1;
//     auto ptr = buffer;
//     while(len--) {
//         switch(*ptr++) {
// #if 0
//             case '+': // next screen
//                 _currentScreen++;
//                 _currentScreen %= numScreens;
//                 _draw();
//                 break;
//             case '-': // prev screen
//                 _currentScreen += numScreens;
//                 _currentScreen--;
//                 _currentScreen %= numScreens;
//                 _draw();
//                 break;
//             case 'd': // redraw
//                 _draw();
//                 break;
//             case 'u': // update weather info
//                 _getWeatherInfo([this](bool status) {
//                     _draw();
//                 });
//                 break;
//             case 'f': // update weather info
//                 _getWeatherForecast([this](bool status) {
//                     _draw();
//                 });
//                 break;
//             case 'g': // gfx debug
//                 _debugDisplayCanvasBorder = !_debugDisplayCanvasBorder;
//                 _draw();
//                 break;
//             case 's':
//                 if (!_isScrolling()) {
//                     _doScroll();
//                     _drawE(_scrollCanvas->getCanvas(), 0);
//                     Scheduler.addTimer(20, true, [this](EventScheduler::TimerPtr timer) {
//                         _scrollTimer(this);
//                     });
//                 }
//                 break;
// #endif
//         }
//     }
// }


// void WeatherStationPlugin::_drawRGBBitmap(int16_t x, int16_t y, uint16_t *pcolors, int16_t w, int16_t h) {
// #if DEBUG_IOT_WEATHER_STATION

//     static int16_t max_height = 0;
//     max_height = std::max(max_height, h);

//     // _tft.drawRGBBitmap(x, y, pcolors, w, h);
//     _broadcastRGBBitmap(x, y, pcolors, w, h);

//     if (_debugDisplayCanvasBorder) {
//         _debug_printf_P(PSTR("WeatherStationPlugin::_drawRGBBitmap(): x=%d, y=%d, w=%u, h=%u (max. %u)\n"), x, y, w, h, max_height);
//         uint16_t colors[] = { ST77XX_CYAN, ST77XX_RED, ST77XX_YELLOW, ST77XX_BLUE };
//         _tft.drawRect(x, y, w, h, colors[y % 4]);
//     }

// #else

//     _tft.drawRGBBitmap(x, y, pcolors, w, h);
//     _broadcastRGBBitmap(x, y, pcolors, w, h);

// #endif
// }

void WeatherStationPlugin::_broadcastCanvas(int16_t x, int16_t y, int16_t w, int16_t h)
{
    return;//TODO
    auto webSocketUI = WsWebUISocket::getWsWebUI();
    if (webSocketUI && !webSocketUI->getClients().isEmpty()) {
        Buffer buffer;

        WebSocketBinaryPacketUnqiueId_t packetIdentifier = RGB565_RLE_COMPRESSED_BITMAP;
        buffer.write(reinterpret_cast<uint8_t *>(&packetIdentifier), sizeof(packetIdentifier));

        const uint8_t len = strlen_P(SPGM(weather_station_webui_id));
        buffer.write(len);
        buffer.write_P(SPGM(weather_station_webui_id), len);

        GFXCanvasRLEStream stream(getCanvas(), x, y, w, h);
        while(stream.available()) {
            buffer.write(stream.read());
        }

        auto wsBuffer = webSocketUI->makeBuffer(buffer.get(), buffer.length());
        buffer.clear();
        if (wsBuffer) {
            wsBuffer->lock();
            for(auto socket: webSocketUI->getClients()) {
                if (socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
                    socket->binary(wsBuffer);
                }
            }
            wsBuffer->unlock();
            webSocketUI->_cleanBuffers();
        }
    }
}
