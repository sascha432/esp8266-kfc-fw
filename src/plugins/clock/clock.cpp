/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <chrono>
#include <ReadADC.h>
#include <stl_ext/algorithm.h>
#include <KFCForms.h>
#include <WebUISocket.h>
#include <async_web_response.h>
#include "clock.h"
#include "web_server.h"
#include "blink_led_timer.h"
#include "../src/plugins/plugins.h"
#include "../src/plugins/sensor/sensor.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include "animation.h"
#include "matrix_validation.h"

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

ClockPlugin ClockPlugin_plugin;

#if IOT_LED_STRIP

#    define PLUGIN_OPTIONS_NAME          "led_strip"
#    define PLUGIN_OPTIONS_FRIENDLY_NAME "RGB LED Strip"

#elif IOT_LED_MATRIX

#    define PLUGIN_OPTIONS_NAME          "led_matrix"
#    define PLUGIN_OPTIONS_FRIENDLY_NAME "LED Matrix"

#else

#    define PLUGIN_OPTIONS_NAME          "clock"
#    define PLUGIN_OPTIONS_FRIENDLY_NAME "Clock"

#endif

#define PLUGIN_OPTIONS_CONFIG_FORMS      "settings,protection,matrix,irremote,ani-*"

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    ClockPlugin,
    PLUGIN_OPTIONS_NAME,
    PLUGIN_OPTIONS_FRIENDLY_NAME,
    "",
    PLUGIN_OPTIONS_CONFIG_FORMS,
    "",
    PluginComponent::PriorityType::MIN,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

ClockPlugin::ClockPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(ClockPlugin)),
    MQTTComponent(ComponentType::LIGHT),
    _schedulePublishState(false),
    _isFading(false),
    _isEnabled(false),
    _forceUpdate(false),
    _isRunning(false),
    _isLocked(false),
    #if IOT_LED_MATRIX_HAVE_PANELS
        _testPattern(TestPatternType::NONE),
        _testPatternSegment(-1),
    #endif
    _display(),
    _lastUpdateTime(0),
    _tempOverride(0),
    _tempBrightness(1.0),
    _fps(0),
    _animMicros(0),
    _showMicros(0),
    _animSum(0),
    _showSum(0),
    _timingFrames(0),
    _timingTimer(0),
    _timerCounter(0),
    _savedBrightness(0),
    _targetBrightness(0),
    _animation(nullptr),
    _blendAnimation(nullptr),
    _method(Clock::ShowMethodType::NONE)
{
    REGISTER_PLUGIN(this, "ClockPlugin");
}

uint8_t getNeopixelShowMethodInt()
{
    return static_cast<uint8_t>(ClockPlugin::getInstance().getShowMethod());
}

MQTT::Json::UnnamedObject ClockPlugin::getWLEDJson()
{
    using MQTT::Json::UnnamedObject;
    using MQTT::Json::NamedObject;
    using MQTT::Json::NamedBool;
    using MQTT::Json::NamedUint32;
    using MQTT::Json::NamedString;
    using MQTT::Json::UnnamedString;
    using MQTT::Json::NamedArray;
    using MQTT::Json::NamedStoredString;
    using MQTT::Json::NamedArray;
    using KFCConfigurationClasses::System;

    UnnamedObject json;

    json.append(
        NamedObject(F("state"),
            NamedBool(F("on"), isEnabled()),
            NamedUint32(F("bri"), _targetBrightness)
        )
    );
    auto mac = WiFi.macAddress();
    mac.replace(F(":"), F(""));
    json.append(
        NamedObject(F("info"),
            NamedString(F("ver"), config.getShortFirmwareVersion()),
            NamedStoredString(F("name"), System::Device::getName()),
            #if ESP8266
                NamedString(F("arch"), F("ESP8266")),
            #elif ESP32
                NamedString(F("arch"), F("ESP32")),
            #endif
            NamedString(F("brand"), F("KFCFW")),
            NamedUint32(F("freeheap"), ESP.getFreeHeap()),
            NamedUint32(F("uptime"), getSystemUptime()),
            NamedStoredString(F("mac"), mac)
        )
    );

    auto effects = NamedArray(F("effects"));
    for(auto i = AnimationType::MIN; i < AnimationType::MAX; i = AnimationType(int(i) + 1)) {
        auto name = _getAnimationName(i);
        if (name) {
            effects.append(UnnamedString(name));
        }
    }
    json.append(effects);

    return json;
}

void ClockPlugin::createMenu()
{
    auto configMenu = bootstrapMenu.getMenuItem(navMenu.config);
    auto subMenu = configMenu.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(F("Settings"), F(LED_MATRIX_MENU_URI_PREFIX "settings.html"));
    // one page per animation, the WebUI uses the same forms inline (ani-<number>.html)
    for(auto i = AnimationType::MIN; i < AnimationType::LAST; i = AnimationType(int(i) + 1)) {
        auto name = _getAnimationName(i);
        if (name) {
            subMenu.addMenuItem(PrintString(F("Animation - %s"), name), PrintString(F(LED_MATRIX_MENU_URI_PREFIX "ani-%s.html"), _getAnimationNameSlug(i)));
        }
    }
    #if IOT_LED_MATRIX_CONFIGURABLE
        subMenu.addMenuItem(F("Matrix"), F(LED_MATRIX_MENU_URI_PREFIX "matrix.html"));
    #endif
    subMenu.addMenuItem(F("Protection"), F(LED_MATRIX_MENU_URI_PREFIX "protection.html"));
    #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
        subMenu.addMenuItem(F("IR Remote"), F(LED_MATRIX_MENU_URI_PREFIX "irremote.html"));
    #endif
}

void ClockPlugin::_setupTimer()
{
    _Timer(_timer).add(Event::seconds(1), true, [this](Event::CallbackTimerPtr timer) {

        _timerCounter++;

        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
            // __LDBG_printf("current power %.3fW", _powerLevel.average / 1000.0);
            // update power level sensor of the webui
            if (_timerCounter % IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE == 0) {
                _updatePowerLevelWebUI();
            }
        #endif

        #if IOT_CLOCK_TEMPERATURE_PROTECTION
            // temperature protection
            if (_timerCounter % kCheckTemperatureInterval == 0) {
                float tempSensor = 0;
                for(auto sensor: SensorPlugin::getInstance().getSensors()) {
                    if (sensor->getType() == MQTT::SensorType::LM75A) {
                        auto &lm75a = *reinterpret_cast<Sensor_LM75A *>(sensor);
                        tempSensor = std::max(tempSensor, lm75a.readSensor()
                        #if IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS != 255
                            - (lm75a.getAddress() == IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS ? _config.protection.regulator_margin : 0)
                        #endif
                        );
                    }
                }
                if (_tempOverride) {
                    tempSensor = _tempOverride;
                }

                if (tempSensor) {

                    // #if DEBUG_IOT_CLOCK
                    //     if (_timerCounter % 60 == 0) {
                    //         __LDBG_printf("temp=%.1f prot=%u range=%u-%u max=%u", tempSensor, isTempProtectionActive(), _config.protection.temperature_reduce_range.min, _config.protection.temperature_reduce_range.max, _config.protection.max_temperature);
                    //         __LDBG_printf("reduce=%u val=%f br=%f", _config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min, _config.protection.temperature_reduce_range.min - tempSensor, std::clamp<float>(1.0 - ((tempSensor - _config.protection.temperature_reduce_range.min) * 0.75 / (float)(_config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min)), 0.25, 1.0));
                    //     }
                    // #endif

                    if (isTempProtectionActive()) {
                        return;
                    }

                    if (tempSensor > _config.protection.max_temperature) {

                        PrintString str;
                        PrintString str2 = F("<span style=\"font-size:1.65rem\">OVERHEATED<br>");

                        for(auto sensor: SensorPlugin::getInstance().getSensors()) {
                            if (sensor->getType() == MQTT::SensorType::LM75A) {
                                auto &lm75a = *reinterpret_cast<Sensor_LM75A *>(sensor);
                                str.printf_P(PSTR("%s %.2f%s "), lm75a.getSensorName().c_str(), lm75a.readSensor(), SPGM(UTF8_degreeC));
                                str2.printf_P(PSTR("%.1f%s"), lm75a.readSensor(), SPGM(UTF8_degreeC));
                            }
                        }
                        _overheatedInfo = std::move(str2);

                        auto message = PrintString(F("Over temperature protection activated: %.1f%s &gt; %u%s"), tempSensor, SPGM(UTF8_degreeC), _config.protection.max_temperature, SPGM(UTF8_degreeC));
                        message += F("<br>");
                        message += str;
                        _setBrightnessLevel(0);
                        enableLoop(false);
                        _tempBrightness = -1.0;
                        #if defined(IOT_CLOCK_HAVE_OVERHEATED_PIN) && IOT_CLOCK_HAVE_OVERHEATED_PIN != -1
                            digitalWrite(IOT_CLOCK_HAVE_OVERHEATED_PIN, LOW);
                            Clock::LedPower::set(false);
                        #endif

                        Logger_error(message);
                        _schedulePublishState = true;
                    }
                    else {
                        // get current brightness
                        uint8_t oldBrightness = _getBrightness();
                        uint8_t newBrightness = oldBrightness;
                        if (tempSensor > _config.protection.temperature_reduce_range.min) {
                            // reduce brightness to 25-100% or 50% if the range is invalid
                            _tempBrightness = (_config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min > 0) ?
                                std::clamp<float>(1.0 - ((tempSensor - _config.protection.temperature_reduce_range.min) * 0.75 / static_cast<float>(_config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min)), 0.25, 1.0) :
                                0.5;
                        }
                        else {
                            _tempBrightness = 1.0;
                        }
                        // check if brightness has changed
                        newBrightness = _getBrightness();
                        if (newBrightness != oldBrightness) {
                            __LDBG_printf("temp. brightness factor=%.2f%% brightness=%u->%u", _tempBrightness * 100.0, oldBrightness, newBrightness);
                            // update the temperature protection sensor
                            _schedulePublishState = true;
                        }
                    }
                }
            }
        #endif

        // mqtt update / webui update
        if (_schedulePublishState || (_timerCounter % kUpdateMQTTInterval == 0)) {
            _schedulePublishState = false;
            _publishState();
        }

    });
}

void ClockPlugin::preSetup(SetupModeType mode)
{
    if (mode == SetupModeType::DEFAULT) {
        auto sensorPlugin = PluginComponent::getPlugin<SensorPlugin>(F("sensor"), false);
        if (sensorPlugin) {
            sensorPlugin->getInstance().setAddCustomSensorsCallback(webUIHook);
        }
    }
}

void ClockPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    #if defined(HAVE_IOEXPANDER)
        auto &_PCF8574 = IOExpander::config._device;
        _PCF8574.DDR = 0b00111111;
        _PCF8574.PORT = 0xff;
    #endif

    Clock::LedPower::set(false);

    _disable();

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        WsClient::addClientCallback(webSocketCallback, this);
    #endif

    readConfig(true);
    _targetBrightness = 0;

    #if PIN_MONITOR

        #if IOT_CLOCK_BUTTON_PIN != -1
            __LDBG_printf("button at pin %u", IOT_CLOCK_BUTTON_PIN);
            pinMonitor.attach<Clock::Button>(IOT_CLOCK_BUTTON_PIN, Clock::ButtonType::MAIN, *this);
        #endif

        #if IOT_CLOCK_TOUCH_PIN != -1
            __LDBG_printf("touch sensor at pin %u", IOT_CLOCK_TOUCH_PIN);
            pinMonitor.attach<Clock::TouchButton>(IOT_CLOCK_TOUCH_PIN, ButtonType::TOUCH, *this);
        #endif

        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            __LDBG_printf("rotary encoder at pin %u,%u active_low=%u", IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB, PIN_MONITOR_ACTIVE_STATE == ActiveStateType::ACTIVE_LOW);
            auto encoder = new Clock::RotaryEncoder(PIN_MONITOR_ACTIVE_STATE);
            encoder->attachPins(IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB);
        #endif

        #if IOT_LED_MATRIX_TOGGLE_PIN != -1
            pinMonitor.attach<Clock::Button>(IOT_LED_MATRIX_TOGGLE_PIN, Clock::ButtonType::TOGGLE_ON_OFF, *this);
        #endif

        #if IOT_LED_MATRIX_INCREASE_BRIGHTNESS_PIN != -1
            pinMonitor.attach<Clock::Button>(IOT_LED_MATRIX_INCREASE_BRIGHTNESS_PIN, Clock::ButtonType::INCREASE_BRIGHTNESS, *this);
        #endif

        #if IOT_LED_MATRIX_DECREASE_BRIGHTNESS_PIN != -1
            pinMonitor.attach<Clock::Button>(IOT_LED_MATRIX_DECREASE_BRIGHTNESS_PIN, Clock::ButtonType::DECREASE_BRIGHTNESS, *this);
        #endif

        #if IOT_LED_MATRIX_NEXT_ANIMATION_PIN != -1
            pinMonitor.attach<Clock::Button>(IOT_LED_MATRIX_NEXT_ANIMATION_PIN, Clock::ButtonType::NEXT_ANIMATION, *this);
        #endif

        if (pinMonitor.getPins().size()) {
            __LDBG_printf("pins attached=%u", pinMonitor.getPins().size());
            pinMonitor.begin();
        }

    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR || IOT_SENSOR_HAVE_MOTION_SENSOR
        for(const auto &sensor: SensorPlugin::getSensors()) {
            switch(sensor->getType()) {
    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        case SensorPlugin::SensorType::AMBIENT_LIGHT: {
                auto lightSensor = reinterpret_cast<Sensor_AmbientLight *>(sensor);
                if (lightSensor->getId() == 0) {
                    #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR == 1
                        auto config = Sensor_AmbientLight::SensorInputConfig(Sensor_AmbientLight::SensorType::INTERNAL_ADC);
                        config.adc.inverted = IOT_CLOCK_AMBIENT_LIGHT_SENSOR_INVERTED;
                    #elif IOT_CLOCK_AMBIENT_LIGHT_SENSOR == 2
                        auto config = Sensor_AmbientLight::SensorInputConfig(Sensor_AmbientLight::SensorType::TINYPWM);
                        config.tinyPWM = Sensor_AmbientLight::SensorInputConfig::TinyPWM(TINYPWM_I2C_ADDRESS, IOT_CLOCK_AMBIENT_LIGHT_SENSOR_INVERTED);
                    #endif
                    lightSensor->begin(this, config);
                }
            }
            break;
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        case SensorPlugin::SensorType::MOTION:
            reinterpret_cast<Sensor_Motion *>(sensor)->begin(this, IOT_CLOCK_MOTION_SENSOR_PIN, IOT_CLOCK_MOTION_SENSOR_PIN_INVERTED);
            break;
    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR || IOT_SENSOR_HAVE_MOTION_SENSOR
                default:
                    break;
            }
        }
    #endif

    MQTT::Client::registerComponent(this);

    enableLoopNoClear(true);

    #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
        _registerIRRemoteWebHandler();
    #endif
    #if IOT_LED_MATRIX_HAVE_PANELS
        _registerMatrixTestWebHandler();
    #endif

    #if IOT_ALARM_PLUGIN_ENABLED
        AlarmPlugin::setCallback(alarmCallback);
    #endif


    // set initial state after reset
    switch(_config.getInitialState()) {
        case InitialStateType::OFF:
            // keep device off after reset
            _savedBrightness = _config.getBrightness();
            _config.enabled = false;
            _setBrightness(0);
            break;
        case InitialStateType::ON:
            // turn device on after reset
            _savedBrightness = std::max<uint8_t>(15, _config.getBrightness()); // restore last brightness but at least ~6%
            _config.enabled = true;
            _setBrightness(_savedBrightness);
            break;
        case InitialStateType::RESTORE:
            // restore last settings
            _savedBrightness = _config.getBrightness();
            if (_config.enabled) {
                _setBrightness(_savedBrightness);
            }
            else {
                _setBrightness(0);
            }
            break;
        default:
            break;
    }

    _setupTimer();
    _isRunning = true;

    #if IOT_LED_MATRIX_SHOW_UPDATE_PROGRESS

        // show update status during OTA updates. the callback runs in the upload handler, the
        // progress is rendered by the loop task, see _showUpdateProgressQueued(). _updateProgress is
        // written by the loop task only, a progress value that is already applied is not queued
        WebServer::Plugin::getInstance().setUpdateFirmwareCallback([this](size_t position, size_t size) {
            const int progress = static_cast<int>(position * 100 / size);
            if (progress == _updateProgress) {
                return;
            }
            _enqueue([this, progress] {
                _showUpdateProgressQueued(progress);
            });
        });

    #endif

}

void ClockPlugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
    if (F("http") == source) {
        // the web server has been restarted, the handlers are gone
        #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
            _registerIRRemoteWebHandler();
        #endif
        #if IOT_LED_MATRIX_HAVE_PANELS
            _registerMatrixTestWebHandler();
        #endif
    }
    const bool applyConfigOnly = source.startsWith(F("ani-"));
    _enqueue([this, applyConfigOnly] { _reconfigureQueued(applyConfigOnly); });
}

void ClockPlugin::_reconfigureQueued(bool applyConfigOnly)
{
    _isRunning = false;
    if (applyConfigOnly) {
        // do not reset just apply new config
        _config.enabled = false;
        _isEnabled = false;
        readConfig(true);
    }
    else {
        // reset entire state and all pixels
        _disable();
        readConfig(false);
    }

    endIRReceiver();
    beginIRReceiver();

    _schedulePublishState = true;
    if (_targetBrightness) {
        _enable();
    }
    _isRunning = true;

    #if IOT_LED_MATRIX_HAVE_PANELS
        // test pattern buttons of the matrix form: the new settings are applied, the next _loop() draws
        // the pattern. This function runs from the task queue at the start of a frame, a pattern drawn
        // here would be overwritten by the animation frame that follows. LEDs that are off are switched
        // on for the pattern (_enable() can refuse: temperature protection), without the _loop() the
        // request would never show up
        if (_testPattern == TestPatternType::AFTER_RECONFIGURE) {
            if (!_isEnabled) {
                _enable();
            }
            _testPattern = _isEnabled ? TestPatternType::SHOW : TestPatternType::NONE;
        }
    #endif
}

#if IOT_LED_MATRIX_HAVE_PANELS

// stops the animation loop and draws the border of every panel with one color per edge (top red,
// bottom blue, left yellow, right green, corners white) to verify the placement, rotation and mirror
// of each segment. brightness 0 uses the configured brightness (the display brightness is still 0 or
// fading in right after the LEDs have been enabled). onlySegment (0 based) draws a single
// segment and leaves all other LEDs off (+LMC=id), -1 draws all. report receives one line per segment
// (+LMC=dir), it may be nullptr. Loop task only
void ClockPlugin::_showTestPattern(uint8_t brightness, const std::function<void(const char *)> &report, int8_t onlySegment)
{
    constexpr uint32_t kTop = 0xff0000;     // red
    constexpr uint32_t kRight = 0x00ff00;   // green
    constexpr uint32_t kBottom = 0x0000ff;  // blue
    constexpr uint32_t kLeft = 0xffff00;    // yellow
    constexpr uint32_t kCorner = 0xffffff;  // white

    enableLoop(false);
    _clear();

    const uint16_t rows = _display.getRows();
    const uint16_t cols = _display.getCols();
    uint32_t linearOffset = 0;

    for (uint8_t i = 0; i < Clock::MatrixValidation::kMaxSegments; i++) {
        const auto &panel = _display.getSegmentPanel(i);
        const uint16_t pixels = _display.getSegmentPixels(i);
        if (!pixels) {
            if (panel.isEnabled() && report) {
                report(PrintString(F("segment %u: panel %ux%u does not fit into the pixel buffer"), i + 1, panel.rows, panel.cols).c_str());
            }
            continue;
        }
        // the linear offset of a segment counts the LEDs of the segments before it
        const uint32_t offset = linearOffset;
        linearOffset += pixels;
        if (onlySegment >= 0 && i != onlySegment) {
            continue;
        }
        if (panel.posRow >= rows || panel.posCol >= cols) {
            if (report) {
                report(PrintString(F("segment %u: panel %ux%u at row=%u col=%u is outside of the matrix"), i + 1, panel.rows, panel.cols, panel.posRow, panel.posCol).c_str());
            }
            continue;
        }
        // the border of the panel, clipped to the matrix. the corners are drawn last, the left column
        // wins over the right one in a single column and the top row over the bottom one in a single row
        const uint16_t minRow = panel.posRow;
        const uint16_t minCol = panel.posCol;
        const uint16_t maxRow = std::min<uint32_t>(rows, static_cast<uint32_t>(panel.posRow) + panel.rows) - 1;
        const uint16_t maxCol = std::min<uint32_t>(cols, static_cast<uint32_t>(panel.posCol) + panel.cols) - 1;
        for (uint16_t col = minCol; col <= maxCol; col++) {
            _display.setPixel(maxRow, col, Clock::PixelRGB(kBottom));
            _display.setPixel(minRow, col, Clock::PixelRGB(kTop));
        }
        for (uint16_t row = minRow; row <= maxRow; row++) {
            _display.setPixel(row, maxCol, Clock::PixelRGB(kRight));
            _display.setPixel(row, minCol, Clock::PixelRGB(kLeft));
        }
        _display.setPixel(minRow, minCol, Clock::PixelRGB(kCorner));
        _display.setPixel(minRow, maxCol, Clock::PixelRGB(kCorner));
        _display.setPixel(maxRow, minCol, Clock::PixelRGB(kCorner));
        _display.setPixel(maxRow, maxCol, Clock::PixelRGB(kCorner));

        if (report) {
            report(PrintString(F("segment %u: panel %ux%u at row=%u col=%u leds=%u linear offset=%u rotation=%u mirror=%u lut=%s"), i + 1, panel.rows, panel.cols, panel.posRow, panel.posCol, pixels, offset, panel.rotation * 90, panel.mirror, _display.isSegmentIdentity(i) ? PSTR("identity") : PSTR("mapped")).c_str());
        }
    }

    // the current brightness, a test pattern at brightness 0 would be invisible: the stored brightness
    // if the LEDs are fading in or were dimmed to 0, a quarter if that is 0 as well
    if (!brightness) {
        brightness = _getBrightness();
    }
    if (!brightness) {
        brightness = _config.brightness;
    }
    if (!brightness) {
        brightness = Clock::kMaxBrightness / 4;
    }
    _display.show(brightness);
    if (report) {
        report(PrintString(F("first row red, last row blue, first column yellow, last column green, corners white, brightness=%u (+LMC=lo,1 resumes the animation)"), brightness).c_str());
    }
}

#endif

#if IOT_LED_MATRIX_SHOW_UPDATE_PROGRESS

// renders the progress of an OTA update. the LEDs are turned on the first time and the animation
// loop is stopped, the progress bar replaces it until the update is done
void ClockPlugin::_showUpdateProgressQueued(int progress)
{
    if (_updateProgress == progress) {
        return;
    }
    if (_updateProgress == -1) {
        // forced on, the progress is shown even if the standby pin is disabled in the configuration
        Clock::LedPower::set(true);
        pinMode(IOT_LED_MATRIX_OUTPUT_PIN, OUTPUT);

        LoopFunctions::remove(standbyLoop);
        _removeLoop();
        _fps = NAN;
    }
    _updateProgress = progress;

    const uint8_t color2 = (progress * progress * progress * progress) / 1000000;
    // use fill to reduce the brightness, show() would adjust the brightness of every pixel
    constexpr uint8_t kBrightnessDivider = 4;
    _display.fill((((100 - progress) / kBrightnessDivider) << 16) | ((color2 / kBrightnessDivider) << 8));
    _display._showNeoPixelEx(255);
}

#endif

void ClockPlugin::shutdown()
{
    // store pending changed on shutdown
    if (_saveTimer) {
        _saveTimer->_callback(nullptr);
        _Timer(_saveTimer).remove();
    }

    _isRunning = false;

    #if PIN_MONITOR
        PinMonitor::GPIOInterruptsDisable();
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR || IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        for(const auto &sensor: SensorPlugin::getSensors()) {
            switch(sensor->getType()) {
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        case SensorPlugin::SensorType::MOTION:
            reinterpret_cast<Sensor_Motion *>(sensor)->end();
            break;
    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        case SensorPlugin::SensorType::AMBIENT_LIGHT:
            if (reinterpret_cast<Sensor_AmbientLight *>(sensor)->getId() == 0) {
                reinterpret_cast<Sensor_AmbientLight *>(sensor)->end();
            }
            break;
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR || IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
                default:
                    break;
            }
        }
    #endif

    #if defined(HAVE_IOEXPANDER)
        auto &_PCF8574 = IOExpander::config._device;
        _PCF8574.DDR = 0b00111111;
        _PCF8574.PORT = 0xff;
    #endif

    #if IOT_CLOCK_HAVE_ROTARY_ENCODER
        _Timer(_rotaryActionTimer).remove();
    #endif

    #if PIN_MONITOR
        pinMonitor.end();
    #endif

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        WsClient::removeClientCallback(this);
    #endif

    _Timer(_timer).remove();

    #if IOT_ALARM_PLUGIN_ENABLED
        _resetAlarm();
        AlarmPlugin::setCallback(nullptr);
    #endif

    if (_targetBrightness && _config.enabled) {
        // limit fade time to 1000ms
        uint16_t max_delay = 1000;
        _setBrightness(0, -1, max_delay);
        max_delay += 75;
        while(_targetBrightness && max_delay--) {
            loop();
            _display.delay(10);
        }
    }

    _removeLoop();

    // turn all LEDs off
    _disable();

    endIRReceiver();

    if (_animation) {
        delete _animation;
        _animation = nullptr;
    }
    if (_blendAnimation) {
        delete _blendAnimation;
        _blendAnimation = nullptr;
    }
}

void ClockPlugin::getStatus(Print &output)
{
    #if IOT_LED_STRIP
        output.print(F("LED Strip Plugin" HTML_S(br)));
    #elif IOT_LED_MATRIX
        output.print(F("LED Matrix Plugin" HTML_S(br)));
    #elif IOT_CLOCK
        output.print(F("Clock Plugin" HTML_S(br)));
    #endif

    #if IOT_CLOCK_TEMPERATURE_PROTECTION
        if (_config.protection.max_temperature > 25 && _config.protection.max_temperature < 85) {
            output.print(F("Over-temperature protection enabled"));
        }
    #endif

    #if IOT_LED_MATRIX
        output.printf_P(PSTR(HTML_S(br) "Total pixels %u"), _display.size());
    #else
        output.printf_P(PSTR(HTML_S(br) "Total pixels %u, digits pixels %u"), Clock::DisplayType::kNumPixels, Clock::SevenSegment::kNumPixelsDigits);
    #endif

    if (_display.getNumSegments() > 1) {
        output.printf_P(PSTR(", segments %u"), _display.getNumSegments());
    }

    switch(Clock::getNeopixelShowMethodType()) {
        #if !HAVE_NEOPIXELBUS
        case Clock::ShowMethodType::FASTLED:
            output.printf_P(PSTR(", FastLED, %.1ffps, dithering %s"), _fps, _display.getDither() ? PSTR("on") : PSTR("off"));
            #if FASTLED_VERSION == 3004000 && (IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION)
                {
                    auto limit = FastLED.getPowerLimitScale();
                    if (limit != 1.0f) {
                        output.printf_P(PSTR(HTML_S(br) "Power limit %.1f%%"), limit * 100.0f);
                    }
                }
            #endif
            break;
        #endif
        #if HAVE_NEOPIXELBUS
            // the transport counters and frame timings are developer details, see +LMC=fr
            case Clock::ShowMethodType::NEOBUS_RMT:
                output.printf_P(PSTR(", NeoPixelBus RMT, %.1ffps"), _fps);
                break;
            case Clock::ShowMethodType::NEOBUS_I2S:
                output.printf_P(PSTR(", NeoPixelBus I2S, %.1ffps"), _fps);
                // the I2S transport needs one free I2S port per segment (the visualizer microphone
                // owns one of the two), the remaining segments are transmitted by the RMT transport
                if (_display.getRmtStrips()) {
                    output.printf_P(PSTR(", %u I2S + %u RMT pins"), _display.getI2sStrips(), _display.getRmtStrips());
                }
                break;
        #endif
        #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
            case Clock::ShowMethodType::NEOPIXEL_EX: {
                #if NEOPIXEL_HAVE_STATS
                        auto &stats = NeoPixelEx::Context::validate(nullptr).getStats();
                        output.printf_P(PSTR(", NeoPixelEx %dfps"), stats.getFps());
                        if (stats.getTime() > 10000) {
                            stats.clear();
                        }
                    #else
                        output.print(F(", NeoPixel"));
                    #endif
                } break;
        #endif
        default:
            break;
    }

    #if IOT_LED_MATRIX_NO_BUTTON
        // nothing to see here move along
    #elif IOT_CLOCK_BUTTON_PIN != -1
        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            #if IOT_CLOCK_TOUCH_PIN
                output.print(F(HTML_S(br) "Rotary encoder with capacitive touch sensor and button"));
            #else
                output.print(F(HTML_S(br) "Rotary encoder with button"));
            #endif
        #else
        #endif
        auto numPins = int(PinMonitor::pinMonitor.getPins().size());
        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            numPins -= 2;
        #endif
        if (numPins > 0) {
            output.printf_P(PSTR(HTML_S(br) "Total buttons: %u"), numPins);
        }
    #endif

    #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
        output.printf_P(PSTR(HTML_S(br) "IR remote control pin %u"), IOT_LED_MATRIX_IR_REMOTE_PIN);
        if (_irFrameCount) {
            output.printf_P(PSTR(HTML_S(br) "Last IR code %08x"), _irLastCode);
        }
        else {
            output.print(F(HTML_S(br) "Last IR code: none received"));
        }
    #endif

    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
        if (_config.isVisualizer()) {
            output.print(F(HTML_S(br)));
            output.print(_getAnimationName(_config.getAnimation()));
            #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
                if (_config.visualizer.input == uint8_t(Clock::VisualizerType::AudioInputType::MICROPHONE)) {
                    output.printf_P(PSTR(", I2S Microphone, Loudness Gain %u, Band Gain %u"), _config.visualizer.mic_loudness_gain, _config.visualizer.mic_band_gain);
                }
                else
            #endif
            {
                output.printf_P(PSTR(", UDP%s Active Port %u"), _config.visualizer.multicast ? PSTR(" Multicast") : PSTR(""), _config.visualizer.port);
            }
        }
    #endif

    #if IOT_CLOCK_TEMPERATURE_PROTECTION
        if (isTempProtectionActive()) {
            output.printf_P(PSTR(HTML_SA(h5, HTML_A("class", "text-danger")) "The temperature exceeded %u" HTML_E(h5)), _config.protection.max_temperature);
        }
    #endif

    #if DEBUG_TASK_QUEUE
        output.printf_P(PSTR(HTML_S(br) "Deferred tasks: %u queued (%u peak of %u), %u processed, %u dropped" HTML_S(br)), static_cast<unsigned>(_tasks.size()), static_cast<unsigned>(_tasks.peakSize()), static_cast<unsigned>(_tasks.capacity()), static_cast<unsigned>(_tasks.processed()), static_cast<unsigned>(_tasks.dropped()));
    #endif
}

void ClockPlugin::setBrightnessDeferred(uint8_t brightness, int ms, uint32_t maxTime)
{
    _enqueue([this, brightness, ms, maxTime] { _setBrightness(brightness, ms, maxTime); });
}

void ClockPlugin::_setBrightness(uint8_t brightness, int ms, uint32_t maxTime)
{
    if (ms < 0) {
        ms = _config.getFadingTimeMillis();
    }
    __LDBG_printf("brightness=%u fading=%u time=%d", brightness, _isFading, ms);
    if (ms == 0 || maxTime == 0) {
        // use _setBrightnessLevel
        _setBrightnessLevel(brightness);
    }
    else {
        // _setBrightnessLevel is called once the fading is complete
        // enable LEDs now if the brightness is not 0, but wait until fading is complete before disabling them
        if (brightness != 0) {
            _enable();
        }
        _startBrightness = _getFadingBrightness();
        _fadingBrightness = ~_getBrightness();
        _targetBrightness = brightness;
        int16_t diff = (int16_t)_startBrightness - (int16_t)_targetBrightness;
        if (diff < 0) {
            diff = -diff;
        }
        __LDBG_printf("from=%u to=%u partial=%d/%d", _startBrightness, _targetBrightness, std::min<uint32_t>(maxTime, ms * diff / Clock::kMaxBrightness), ms);
        // calculate time relative to the level change
        _fadeTimer.set(std::min<uint32_t>(maxTime, ms * diff / Clock::kMaxBrightness));
        _isFading = true;
        _updateBrightnessSettings();
        // publish new state immediately
        _schedulePublishState = true;
    }
}

void ClockPlugin::setAnimationDeferred(AnimationType animation, uint16_t blendTime)
{
    _enqueue([this, animation, blendTime] {
        _setAnimation(animation, blendTime);
    });
}

// copies the visualizer mode of an animation into VisualizerType::type
void ClockPlugin::_syncVisualizerType(AnimationType animation)
{
    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
        const auto type = ClockConfigType::getVisualizerType(animation);
        if (type != VisualizerAnimationType::MAX && type != _config.visualizer.get_enum_type(_config.visualizer)) {
            __LDBG_printf("animation=%u visualizer_type=%u", static_cast<unsigned>(animation), static_cast<unsigned>(type));
            _config.visualizer.type = static_cast<uint8_t>(type);
        }
    #endif
}

void ClockPlugin::_setAnimation(AnimationType animation, uint16_t blendTime)
{
    __LDBG_printf("animation=%d blend_time=%u", animation, blendTime);
    // the animation is created and published here, loop task only, see _publishAnimation()
    #if IOT_LED_MATRIX == 0
        switch(animation) {
            case AnimationType::COLON_SOLID:
                _setBlinkColon(0);
                return;
            case AnimationType::COLON_BLINK_SLOWLY:
                _setBlinkColon(1000);
                return;
            case AnimationType::COLON_BLINK_FAST:
                _setBlinkColon(0);
                return;
            default:
                break;
        }
    #endif
    _config.animation = static_cast<uint8_t>(animation);
    switch(animation) {
        case AnimationType::FADING:
            _publishAnimation(new Clock::FadingAnimation(*this, _getColor(), Color().rnd(), _config.fading.speed, _config.fading.delay * 1000), blendTime);
            break;
        case AnimationType::RAINBOW:
            _publishAnimation(new Clock::RainbowAnimation(*this, _config.rainbow.speed, _config.rainbow.multiplier, _config.rainbow.color), blendTime);
            break;
        case AnimationType::RAINBOW_FASTLED:
            _publishAnimation(new Clock::RainbowAnimationFastLED(*this, _config.rainbow.bpm, _config.rainbow.hue, _config.rainbow.invert_direction), blendTime);
            break;
        case AnimationType::FLASHING:
            _publishAnimation(new Clock::FlashingAnimation(*this, _getColor(), _config.flashing_speed), blendTime);
            break;
        case AnimationType::FIRE:
            _publishAnimation(new Clock::FireAnimation(*this, _config.fire), blendTime);
            break;
        case AnimationType::PLASMA:
            _publishAnimation(new Clock::PlasmaAnimation(*this, _getColor(), _config.plasma), blendTime);
            break;
        case AnimationType::XMAS:
            _publishAnimation(new Clock::XmasAnimation(*this, _config.xmas), blendTime);
            break;
        case AnimationType::GRADIENT:
            _publishAnimation(new Clock::GradientAnimation(*this, _config.gradient), blendTime);
            break;
        #if IOT_LED_MATRIX_ENABLE_VISUALIZER
            // the animation is the mode selector and VisualizerType::type mirrors it
            case AnimationType::VISUALIZER_SPECTRUM_RAINBOW_BARS:
            case AnimationType::VISUALIZER_SPECTRUM_GRADIENT_BARS:
            case AnimationType::VISUALIZER_SPECTRUM_SINGLE_COLOR_BARS:
            case AnimationType::VISUALIZER_PLASMA_REACTIVE:
            case AnimationType::VISUALIZER_FIRE_REACTIVE:
                _syncVisualizerType(animation);
                _publishAnimation(new Clock::VisualizerAnimation(*this, _getColor(), _config.visualizer), blendTime);
                break;
        #endif
        case AnimationType::SOLID:
        default:
            _config.animation = static_cast<uint8_t>(AnimationType::SOLID);
            _setColor(_config.solid_color);
            _publishAnimation(new Clock::SolidColorAnimation(*this, _getColor()), blendTime);
            break;
    }
    _schedulePublishState = true;
}

// Sanitize the values that program the LED driver and the pixel mapping. The config is written by
// the web server task (form callbacks) while this runs in the loop task, so a torn value must not
// reach ESP32RMTController::deinit() (see _setShowMethod) or the LED buffer.
void ClockPlugin::_sanitizeConfig()
{
    _config.protection.max_temperature = std::max<uint8_t>(kMinimumTemperatureThreshold, _config.protection.max_temperature);

    #if IOT_LED_MATRIX_FASTLED_ONLY
        _config.method = static_cast<uint8_t>(Clock::ShowMethodType::FASTLED);
    #else
        #if HAVE_NEOPIXELBUS
            // the NeoPixelBus env has no FastLED transport, the stored FastLED default is remapped
            if (_config.method == static_cast<uint8_t>(Clock::ShowMethodType::FASTLED) || _config.method > IOT_CLOCK_SHOW_METHOD_MAX) {
                __DBG_printf("invalid show method %u, using NeoPixelBus RMT", _config.method);
                _config.method = static_cast<uint8_t>(Clock::ShowMethodType::NEOBUS_RMT);
            }
        #else
            if (_config.method > IOT_CLOCK_SHOW_METHOD_MAX) {
                __DBG_printf("invalid show method %u, using FastLED", _config.method);
                _config.method = static_cast<uint8_t>(Clock::ShowMethodType::FASTLED);
            }
        #endif
    #endif

    #if IOT_LED_MATRIX_CONFIGURABLE
        _sanitizeMatrixConfig(_config.matrix);
    #endif
}

#if IOT_LED_MATRIX_CONFIGURABLE

void ClockPlugin::_sanitizeMatrixConfig(MatrixConfigType &matrix)
{
    // the layout has to stay inside the pixel buffer, updateSegments() does not validate it - the rules
    // are shared with the matrix form, see matrix_validation.h. rows/cols = 0 would divide by zero and a
    // mapping that exceeds the buffer would read outside of it, PixelMapping::setParams() takes
    // rowOfs/colOfs modulo them
    if (!matrix.rows || !matrix.cols || static_cast<uint32_t>(matrix.rows) * matrix.cols > Clock::MatrixValidation::kMaxPixels) {
        __DBG_printf("invalid matrix %ux%u, using defaults", matrix.rows, matrix.cols);
        matrix.rows = IOT_LED_MATRIX_ROWS;
        matrix.cols = IOT_LED_MATRIX_COLS;
    }
    #if IOT_LED_MATRIX_HAVE_PANELS
        // every segment is a panel of at least 1x1 LEDs (a zero filled storage has 0x0), a panel that does
        // not fit into the pixel buffer is disabled
        for(uint8_t i = 0; i < Clock::MatrixValidation::kMaxSegments; i++) {
            auto panel = matrix.getPanel(i);
            if (!panel.rows || !panel.cols) {
                panel.rows = MatrixConfigType::kDefaultValueFor_panel_rows0;
                panel.cols = MatrixConfigType::kDefaultValueFor_panel_cols0;
            }
            if (static_cast<uint32_t>(panel.rows) * panel.cols > Clock::MatrixValidation::kMaxPixels) {
                __DBG_printf("segment %u: panel %ux%u exceeds the pixel buffer, disabled", i + 1, panel.rows, panel.cols);
                panel.enabled = false;
            }
            matrix.setPanel(i, panel);
        }
    #endif
}

#endif

// reads the stored configuration and applies it, loop task only
void ClockPlugin::readConfig(bool setup)
{
    // read config
    _config = Plugins::Clock::getConfig();
    _sanitizeConfig();

    #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
        // The IR remote configuration is new. If the stored data predates it, the storage has been
        // resized and zero filled (see WriteableData::resize). The form reads the storage, therefore
        // the defaults (receiver enabled, no button assigned) are stored once
        if (!Plugins::Clock::getWriteableConfig().ir.isInitialized()) {
            __LDBG_printf("applying the IR remote config defaults");
            _config.ir.applyDefaults();
            _saveState();
        }
    #endif

    _setShowMethod(static_cast<Clock::ShowMethodType>(_config.method));

    #if IOT_LED_MATRIX_CONFIGURABLE
        // the segments are built with the pixel mapping, it has to be set first
        if (!_display.setParams(_config.matrix.rows, _config.matrix.cols, _config.matrix.reverse_rows, _config.matrix.reverse_cols, _config.matrix.rotate, _config.matrix.interleaved, _config.matrix.rowOfs, _config.matrix.colOfs)) {
            __DBG_printf("_display.setParams() failed");
        }
    #endif
    #if IOT_LED_MATRIX_HAVE_PANELS
        // one panel per segment, see Clock::SegmentPanel
        Clock::SegmentPanel panels[Clock::MatrixValidation::kMaxSegments];
        for(uint8_t i = 0; i < Clock::MatrixValidation::kMaxSegments; i++) {
            const auto panel = _config.matrix.getPanel(i);
            panels[i] = Clock::SegmentPanel(panel.enabled, panel.rows, panel.cols, panel.posRow, panel.posCol, panel.rotation, panel.mirror);
        }
        _display.setSegmentPanels(panels);
    #endif
    _display.updateSegments();

    _display.setDither(_config.dithering);

    #if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        __LDBG_printf("limit=%u/%u r/g/b/idle=%u/%u/%u/%u", _config.power_limit, _getPowerLevelLimit(_config.power_limit), _config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
            _powerLevel.clear();
        #endif

        #if HAVE_NEOPIXELBUS
            // NeoPixelBus has no power management, the limit is applied by _loop(), see
            // _getNeoBusBrightness()
        #else
            #if FASTLED_VERSION >= 3005000
                // UNUSED: no env uses the official FastLED as transport, all FastLED envs use the fork (3.4.0, #else).
                // Kept for the power management of the official library, it does not compile with 3.9.x as is
                // (m_pPowerFunc is private there)
                if (_getPowerLevelLimit(_config.power_limit) == ~0U) {
                    FastLED.m_pPowerFunc = nullptr; // if this does not compile, make m_pPowerFunc public in FastLED.h
                }
                else {
                    FastLED.setMaxPowerInMilliWatts(_getPowerLevelLimit(_config.power_limit));
                    FastLED.m_pPowerFunc = calcPowerFunction;
                }
            #else
                if (_getPowerLevelLimit(_config.power_limit) == ~0U) {
                    FastLED.setMaxPowerInMilliWatts(0);
                }
                else {
                    FastLED.setPowerConsumptionInMilliWattsPer256(_config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
                    FastLED.setMaxPowerInMilliWatts(_getPowerLevelLimit(_config.power_limit), &calcPowerFunction);
                }
            #endif
        #endif
    #endif

    // reset temperature protection
    _tempBrightness = 1.0;
    #if IOT_CLOCK_HAVE_OVERHEATED_PIN
        digitalWrite(IOT_CLOCK_HAVE_OVERHEATED_PIN, HIGH);
    #endif

    if (!_config.standby_led) {
        Clock::LedPower::release();
    }

    #if defined(IOT_LED_MATRIX_STANDBY_LED_PIN) && IOT_LED_MATRIX_STANDBY_LED_PIN != -1
        // enable/disable extra LED that is powered by the 5V from the relay/mosfet
        // 1 pulls the LED to GND, 0 leaves it floating
        pinMode(IOT_LED_MATRIX_STANDBY_LED_PIN, OUTPUT);
        digitalWrite(IOT_LED_MATRIX_STANDBY_LED_PIN, IOT_LED_MATRIX_STANDBY_LED_PIN_LEVEL);
    #endif

    beginIRReceiver();

    _setColor(_config.solid_color.value);
    _setBrightnessLevel(_config.getBrightness(), false);
    _setAnimation(_config.getAnimation());
}

void ClockPlugin::_setBrightnessLevel(uint8_t brightness, bool useEnable)
{
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        #if IOT_ALARM_PLUGIN_ENABLED
            AlarmPlugin::resetAlarm();
        #endif
        return;
    }
    __LDBG_printf("brightness=%u fading=%u", brightness, _isFading);
    if (useEnable) {
        if (brightness) {
            _enable();
        }
        else {
            _disable();
        }
    }
    _updateBrightnessSettings();
    _targetBrightness = brightness;
    _fadeTimer.disable();
    _forceUpdate = true;
    _isFading = false;
    _schedulePublishState = true;
}

void ClockPlugin::_enable()
{
    if (_isLocked) {
        _isLocked = false;
        return;
    }
    if (_isEnabled) {
        __LDBG_printf("enable LED SKIPPED (is_enabled=%u, config=%u)", _isEnabled, _config.enabled);
        return;
    }
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        return;
    }

    __LDBG_printf("enable LED (is_enabled=%u, config=%u)", _isEnabled, _config.enabled);
    _setLedPower(true);

    enableLoopNoClear(true);

    _config.enabled = true;
    _isEnabled = true;

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        _powerLevel.clear();
    #endif

    #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
        NeoPixelEx::Context::validate(nullptr).getStats().clear();
    #endif
    #if FASTLED_DEBUG_COUNT_FRAME_RETRIES
        extern uint32_t _frame_cnt;
        extern uint32_t _retry_cnt;
        _frame_cnt = 0;
        _retry_cnt = 0;
    #endif
}

void ClockPlugin::_disable()
{
    __LDBG_printf("disable LED (is_enabled=%u, config=%u)", _isEnabled, _config.enabled);

    // turn all leds off and set brightness to 0
    _display.setBrightness(0);
    _display.clear();
    _display_show();

    _config.enabled = false;
    _isEnabled = false;

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        _calcPowerFunction(0, 0);
    #endif

    _setLedPower(false);

    _removeLoop();
    LOOP_FUNCTION_ADD(standbyLoop);
}

#if IOT_ALARM_PLUGIN_ENABLED

void ClockPlugin::_alarmCallback(ModeType mode, uint16_t maxDuration)
{
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        _resetAlarm();
        return;
    }
    if (maxDuration == Alarm::STOP_ALARM) {
        _resetAlarm();
        return;
    }

    // alarm reset function already set?
    if (!_resetAlarmFunc) {

        if (_isFading) {
            // stop fading
            // the target brightness will be set after the alarm has been disabled
            _setBrightnessLevel(_targetBrightness);
        }

        struct {
            AnimationType animation;
            uint8_t targetBrightness;
            bool autoBrightness;
        } saved = {
            _config.getAnimation(),
            _targetBrightness,
            #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
                isAutoBrightnessEnabled()
            #else
                false
            #endif
        };

        __LDBG_printf("storing parameters brightness=%u auto=%d color=%s animation=%u", saved.targetBrightness, saved.autoBrightness, getColor().toString().c_str(), saved.animation);
        _resetAlarmFunc = [this, saved](Event::CallbackTimerPtr timer) {
            #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
                setAutoBrightness(saved.autoBrightness);
            #endif
            _targetBrightness = saved.targetBrightness;
            _setAnimation(saved.animation);
            __LDBG_printf("restored parameters brightness=%u auto=%d color=%s animation=%u timer=%u", saved.targetBrightness, saved.autoBrightness, getColor().toString().c_str(), saved.animation, (bool)timer);
            timer->disarm();
            _resetAlarmFunc = nullptr;
        };
    }

    // check if an alarm is already active
    if (!_alarmTimer) {
        __LDBG_printf("alarm brightness=%u color=%s", _targetBrightness, getColor().toString().c_str());
        #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
            setAutoBrightness(false);
        #endif
        _targetBrightness = kMaxBrightness;
        _publishAnimation(new Clock::FlashingAnimation(*this, _config.alarm.color.value, _config.alarm.speed));
    }

    if (maxDuration == 0) {
        maxDuration = Alarm::DEFAULT_MAX_DURATION;
    }
    // reset time if alarms overlap
    __LDBG_printf("alarm duration %u", maxDuration);
    _Timer(_alarmTimer).add(Event::seconds(maxDuration), false, _resetAlarmFunc);
}

#endif

#if IOT_CLOCK_OPTIMIZE_LOOP
#    pragma GCC push_options
#    pragma GCC optimize ("O3")
#endif

void IRAM_ATTR ClockPlugin::_loop()
{
    // the loop task is the only one that may change the display or the config, it applies all
    // changes requested by other tasks here
    _applyPendingChanges();

    #if IOT_LED_MATRIX_HAVE_PANELS
        // the pattern replaces the animation, _showTestPattern() stops the loop
        if (_testPattern == TestPatternType::SHOW) {
            _testPattern = TestPatternType::NONE;
            _showTestPattern(0, nullptr, _testPatternSegment);
            return;
        }
    #endif

    const uint32_t start = micros();

    LoopOptionsType options(*this);
    #if HAVE_NEOPIXELBUS && IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        // FastLED applied the power limit inside show(), NeoPixelBus has no power management, the
        // brightness is limited here before the frame is transmitted
        _display.setBrightness(_calcPowerLevel(_getNeoBusBrightness(_getBrightness(), _getPowerLevelLimit(_config.power_limit))));
    #else
        _display.setBrightness(_getBrightness());
    #endif

    #if HAVE_NEOPIXELBUS
        if (_method == Clock::ShowMethodType::NEOBUS_RMT || _method == Clock::ShowMethodType::NEOBUS_I2S) {
            _fps = _display.getFps();
        }
    #else
        #if IOT_LED_MATRIX_FASTLED_ONLY == 0
            if (_method == Clock::ShowMethodType::FASTLED)
        #endif
        {
            auto frames = std::clamp<uint32_t>(_fps * 10, 100, 0xffffff);
            auto fps = FastLED.getFPS();
            if (fps) {
                _fps = ((_fps * frames) + fps) / (frames + 1.0);
            }
        }
    #endif

    if (_animation) {
        _animation->loop(options.getMillis());
    }
    if (_blendAnimation) {
        _blendAnimation->loop(options.getMillis());
    }

    if (options.doUpdate()) {
        _loopDoUpdate(options);
    }

    // the frame time is the animation work plus the transport, averaged over the last second (a
    // single frame is too noisy, the loop is interrupted by higher priority tasks)
    _animSum += micros() - start;

    const uint32_t showStart = micros();
    _display_show();
    _showSum += micros() - showStart;

    _timingFrames++;
    const uint32_t now = millis();
    if ((now - _timingTimer) >= 1000) {
        _animMicros = _animSum / _timingFrames;
        _showMicros = _showSum / _timingFrames;
        _animSum = 0;
        _showSum = 0;
        _timingFrames = 0;
        _timingTimer = now;
    }
}

// use O3 for the show function on ESP8266 and keep all code in IRAM since it is called very frequently
#if ESP8266
#    pragma GCC push_options
#    pragma GCC optimize ("O3")
#endif

// the display is only rendered by the loop task
void IRAM_ATTR ClockPlugin::_display_show()
{
    _display.show();
}

#if ESP8266
#   pragma GCC pop_options
#endif

// queue a request, see the "Requests" section in clock.h. returns false if the queue is full
bool ICACHE_FLASH_ATTR ClockPlugin::_enqueue(TaskQueue::Task task)
{
    if (!task) {
        return false;
    }
    if (_tasks.push(std::move(task)) != TaskQueue::ResultType::SUCCESS) {
        #if DEBUG_TASK_QUEUE
            __DBG_printf_E("task queue full, size=%u dropped=%u", static_cast<unsigned>(_tasks.size()), static_cast<unsigned>(_tasks.dropped()));
        #else
            __LDBG_printf_E("task queue full, size=%u", static_cast<unsigned>(_tasks.size()));
        #endif
        return false;
    }
    return true;
}

// execute the queued requests. the loop task drains the queue once per iteration, outside of
// __Scheduler.run(), so a request may add/remove timers and loop functions safely
void ICACHE_FLASH_ATTR ClockPlugin::_applyPendingChanges()
{
    _tasks.process();
}

// the form (web task) writes into the storage, adopt it. only the loop task touches _config and
// the save timer, see createConfigureForm()
void ICACHE_FLASH_ATTR ClockPlugin::_syncConfigFromStorageQueued()
{
    _config = Plugins::Clock::getWriteableConfig();
    // everything has been written already, remove the delayed save
    _Timer(_saveTimer).remove();
}

// keep all the animation code in flash memory
void ICACHE_FLASH_ATTR ClockPlugin::_loopDoUpdate(LoopOptionsType &options)
{
    // start update process
    _lastUpdateTime = millis();

    // check if we need to update the animation
    if (options.doRedraw()) {

        #if IOT_LED_MATRIX == 0
            // we can use a boolean here, the compiler seems to optimize the code and removes the if else in the setColons() call
            bool displayColon = true;

            // does the animation allow blinking and is it set?
            if ((!_animation || (_animation && !_animation->isBlinkColonDisabled())) && (_config.blink_colon_speed >= kMinBlinkColonSpeed)) {
                // set on/off depending on the update rate
                displayColon = ((_lastUpdateTime / _config.blink_colon_speed) % 2 == 0);
            }

            auto &tm = options.getLocalTime();
            auto tm_hour = tm.tm_hour_format(); // get hour in 12/24h format

            _display.setDigit(0, tm_hour / 10);
            _display.setDigit(1, tm_hour % 10);
            _display.setDigit(2, tm.tm_min / 10);
            _display.setDigit(3, tm.tm_min % 10);
            #if IOT_CLOCK_NUM_DIGITS == 6
                _display.setDigit(4, tm.tm_sec / 10);
                _display.setDigit(5, tm.tm_sec % 10);
            #endif

            _display.setColons(displayColon ? Clock::SevenSegment::ColonType::BOTH : Clock::SevenSegment::ColonType::NONE);
        #endif

        if (_blendAnimation) {
            // generate new frames for both animations
            _animation->nextFrame(options.getMillis());
            _blendAnimation->nextFrame(options.getMillis());

            // blend frames together into _display
            if (!_blendAnimation->blend(_display, options.getMillis())) {
                __LDBG_printf("blending done");
                delete _blendAnimation;
                _blendAnimation = nullptr;
            }
        }
        else if (_animation) {
            // create next frame and copy it to display
            _animation->nextFrame(options.getMillis());
            _animation->copyTo(_display, options.getMillis());
        }

        #if IOT_LED_MATRIX_GROUP_PIXELS

            if (_config.group_pixels > 0) {

                // number of steps
                const size_t step = _config.group_pixels;
                // number of pixels
                const size_t num = step * 3;
                // fraction to blend of each pixel
                const uint8_t blendFraction = 255 / num;

                // interval in milliseconds to cycle through the 3 pixels
                constexpr uint32_t kInterval = 5000;
                // make sure it is dividable by 256 and more than 1
                constexpr uint32_t kInterval256 = std::max(1, int((kInterval + 255) / 256.0));
                // convert millis into our current relative position
                const uint32_t n = options.getMillis() / kInterval256;

                // scaling state
                const size_t bp = n / 256;
                // scaling value
                const size_t bn = n % 256;

                // scaling values for each pixel
                std::array<uint8_t, 3> scale;
                auto iterator = scale.begin() + (bp % 3);
                *iterator = 0;
                if (++iterator == scale.end()) {
                    iterator = scale.begin();
                }
                *iterator = ~bn;
                if (++iterator == scale.end()) {
                    iterator = scale.begin();
                }
                *iterator = bn;

                // pixels range
                auto pixels = _display.begin();
                const auto endPtr = _display.end();
                while(pixels < endPtr) {
                    // read pixel group and blend all into 3 pixels
                    auto px = pixels;
                    // blend colors
                    auto color = *pixels;
                    *pixels++ = 0;
                    for (size_t j = 1; j < num && pixels < endPtr; j++) {
                        Clock::nblend(color, *pixels, blendFraction);
                        *pixels++ = 0;
                    }
                    // pixels is the end pointer for this group
                    if (px < pixels) {
                        // color = 0xff0000;
                        *px = Clock::PixelRGB(color).nscale8(scale[0]);
                    }
                    if ((px = px + step) < pixels) {
                        // color = 0x00ff00;
                        *px = Clock::PixelRGB(color).nscale8(scale[1]);
                    }
                    if ((px = px + step) < pixels) {
                        // color = 0x0000ff;
                        *px = color.nscale8(scale[2]);
                    }
                }
            }
        #endif
    }
}

#if IOT_CLOCK_OPTIMIZE_LOOP
#    pragma GCC pop_options
#endif

// crash context: called by the ESP8266 crash callback (KFCLibrary save_crash.cpp, the name is kept for it),
// switches the LED power off with register writes only, see led_power.h
void ClockPluginClearPixels()
{
    Clock::LedPower::crashOff();
}

// restart: blanks the pixels with the transport (ESP8266, the ESP32 RMT transport is not used here) and
// switches the LED power off
void ClockPluginShutdownPixels()
{
    #if !ESP32
        ClockPlugin::clearPixels();
    #endif
    Clock::LedPower::crashOff();
}
