/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <MillisTimer.h>
#include <vector>
#include "WebUIComponent.h"
#include "animation.h"
#include "clock_button.h"
#include "kfc_fw_config.h"
#include "plugins.h"
#include "stored_state.h"
#include "../src/plugins/plugins.h"
#if IOT_LED_MATRIX_HAVE_SSD1306
#include <Adafruit_SSD1306.h>
#endif

namespace WebServer {
    class AsyncUpdateWebHandler;
}
class ClockPlugin;

namespace Clock {

    #if DEBUG_MEASURE_ANIMATION
    struct StatsValue {
        uint32_t min;
        uint32_t max;
        float avg;

        StatsValue() : min(~0U), max(0), avg(NAN) {}

        void add(uint32_t value) {
            if (isnan(avg)) {
                avg = value;
            }
            else {
                avg = (avg + value) * 0.5;
            }
            min = std::min<uint32_t>(min, value);
            max = std::max<uint32_t>(max, value);
        }

        void clear() {
            *this = StatsValue();
        }

        void dump(Print &print, const __FlashStringHelper *name) {
            print.printf_P(PSTR("%s: min=%u(%.1f) max=%u(%.1f) avg=%.1f(%.1f)\n"),
                name,
                min, clockCyclesToMicroseconds(static_cast<float>(min)),
                max, clockCyclesToMicroseconds(static_cast<float>(max)),
                avg, clockCyclesToMicroseconds(avg)
            );
        }
    };

    struct AnimationStats {

        StatsValue loop;
        StatsValue nextFrame;
        StatsValue copyTo;

        AnimationStats() {}

        void dump(Print &print) {
            loop.dump(print, F("loop"));
            nextFrame.dump(print, F("nextFrame"));
            copyTo.dump(print, F("copyTo"));
        }

        void clear() {
            loop.clear();
            nextFrame.clear();
            copyTo.clear();
        }
    };

    extern AnimationStats animationStats;

    #endif

    class LoopOptionsBase
    {
    public:
        LoopOptionsBase(ClockPlugin &plugin);

        uint32_t getTimeSinceLastUpdate() const;
        uint32_t getMillis() const;
        uint8_t getBrightness() const;
        bool doUpdate() const;
        bool doRedraw();
        time_t getNow() const;

    protected:
        // uint16_t _updateRate;
        bool &_forceUpdate;
        uint8_t _brightness;
        uint32_t _millis;
        uint32_t _millisSinceLastUpdate;
    };

    inline uint32_t LoopOptionsBase::getTimeSinceLastUpdate() const
    {
        return _millisSinceLastUpdate;
    }

    inline uint32_t LoopOptionsBase::getMillis() const
    {
        return _millis;
    }

    inline uint8_t LoopOptionsBase::getBrightness() const
    {
        return _brightness;
    }

    inline bool LoopOptionsBase::doUpdate() const
    {
        return (_millisSinceLastUpdate >= kUpdateRate);
    }

    inline bool LoopOptionsBase::doRedraw()
    {
        if (_forceUpdate || doUpdate()) {
            _forceUpdate = false;
            return true;
        }
        return false;
    }

    inline time_t LoopOptionsBase::getNow() const
    {
        return time(nullptr);
    }

    using LEDMatrixLoopOptions = LoopOptionsBase;

    class ClockLoopOptions : public LoopOptionsBase {
    public:
        struct tm24 : tm {
            using tm::tm;
            int tm_hour_format() const {
                return tm_format_24h ? tm_hour : ((tm_hour + 23) % 12) + 1;
            }
            void set_format_24h(bool format_24h) {
                tm_format_24h = format_24h;
            }
            bool tm_format_24h{true};
            tm24 &operator=(struct tm *_tm) {
                static_cast<struct tm &>(*this) = *_tm;
                return *this;
            }
            tm24 &operator=(const struct tm &_tm) {
                static_cast<struct tm &>(*this) = _tm;
                return *this;
            }
        } tm24;

    public:
        ClockLoopOptions(ClockPlugin &plugin);

        time_t getNow() const;
        struct tm24 &getLocalTime(time_t *nowPtr = nullptr);
        bool hasTimeChanged() const;
        bool doRedraw();

    protected:
        time_t &_time;
        time_t _now;
        struct tm24 _tm;
        bool format_24h;
    };

    inline time_t ClockLoopOptions::getNow() const
    {
        return _now;
    }

    inline bool ClockLoopOptions::hasTimeChanged() const
    {
        return _now != _time;
    }

    inline bool ClockLoopOptions::doRedraw()
    {
        if (_forceUpdate || doUpdate() || hasTimeChanged()) {
            _forceUpdate = false;
            _time = _now;
            return true;
        }
        return false;
    }

#if IOT_LED_MATRIX
    using LoopOptionsType = LEDMatrixLoopOptions;
#else
    using LoopOptionsType = ClockLoopOptions;
#endif

}

class ClockPlugin : public PluginComponent, public MQTTComponent
#if IOT_SENSOR_HAVE_MOTION_SENSOR
    , public MotionSensorHandler
#endif
#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
    , public AmbientLightSensorHandler
#endif
{
public:

    using Plugins = KFCConfigurationClasses::PluginsType;

    // using SevenSegmentDisplay = Clock::SevenSegmentDisplay;
    using ClockConfigType = Clock::ClockConfigType;
    using Color = Clock::Color;
    using AnimationType = Clock::AnimationType;
    using InitialStateType = Clock::InitialStateType;
    using StoredState = Clock::StoredState;
    using LoopOptionsType = Clock::LoopOptionsType;
    using milliseconds = std::chrono::duration<uint32_t, std::ratio<1>>;
    using seconds = std::chrono::duration<uint32_t, std::ratio<1000>>;
    using NamedArray = PluginComponents::NamedArray;
    using VisualizerType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerAnimationType::VisualizerType;
    using FireAnimationType = KFCConfigurationClasses::Plugins::ClockConfigNS::FireAnimationType;
    using RainbowConfigType = KFCConfigurationClasses::Plugins::ClockConfigNS::RainbowAnimationType;
    using RainbowMultiplierType = RainbowConfigType::MultiplierType;
    using RainbowColorType = RainbowConfigType::ColorAnimationType;

    static constexpr uint16_t kDefaultUpdateRate  = 1000;  // milliseconds
    static constexpr uint16_t kMinBlinkColonSpeed = 50;
    static constexpr uint16_t kMinFlashingSpeed   = 50;
    static constexpr uint16_t kMinRainbowSpeed    = 1;

    static constexpr uint8_t kUpdateAutoBrightnessInterval = 2;  // seconds
    static constexpr uint8_t kCheckTemperatureInterval     = 5;  // seconds
    static constexpr uint8_t kMinimumTemperatureThreshold  = 30;  // °C
    static constexpr uint8_t kUpdateMQTTInterval           = 30;  // seconds

    static constexpr uint8_t kMaxBrightness = Clock::kMaxBrightness;

    static constexpr int16_t kAutoBrightnessOff = -1;

    constexpr static uint8_t kEnablePinState(bool active) {
        return IF_IOT_LED_MATRIX_ENABLE_PIN_INVERTED(active ? LOW : HIGH, active ? HIGH : LOW);
    }

    enum class DisplaySensorType : uint8_t {
        OFF  = 0,
        SHOW = 1,
        BUSY = 2,  // waiting for ADC results
        DESTROYED = 3,
    };

// ------------------------------------------------------------------------
// PluginComponent
// ------------------------------------------------------------------------
public:
    ClockPlugin();

    virtual void preSetup(SetupModeType mode) override;
    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;
    virtual void createMenu() override;

    enum class TitleType {
        NONE,
        ADD_GROUP,          // surrounds the form with a group/animation title
        SET_TITLE           // set FormUI title to the animation title
    };

    void _createConfigureFormAnimation(AnimationType animation, FormUI::Form::BaseForm &form, ClockConfigType &cfg, TitleType titleType);

    #if AT_MODE_SUPPORTED

    private:
        #if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
            struct LedMatrixDisplayTimer {
                Event::Timer timer;
                void *clientId;
                String host;
                uint16_t udpPort;
                uint16_t wsPort;
                uint32_t errors;
                uint32_t maxErrors;
                LedMatrixDisplayTimer(void *pClientId, uint32_t pMaxErrors) : clientId(pClientId), udpPort(0), wsPort(0), errors(0), maxErrors(pMaxErrors) {
                }
                ~LedMatrixDisplayTimer() {
                    timer.remove();
                    clientId = nullptr;
                }
                void print(const String &str) {
                    auto client = Http2Serial::getClientById(clientId);
                    if (client) {
                        client->text(str);
                    }
                    else {
                        Serial.println(str);
                    }
                }
            };
            LedMatrixDisplayTimer *_displayLedTimer{nullptr};

            void _removeDisplayLedTimer();
        #endif

    public:
        #if AT_MODE_HELP_SUPPORTED
            virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
        #endif
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

// ------------------------------------------------------------------------
// WebUI
// ------------------------------------------------------------------------
public:
    virtual void createWebUI(WebUINS::Root &webUI) override {}
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;
    // virtual bool getValue(const String &id, String &value, bool &state) override;

    void _createWebUI(WebUINS::Root &webUI);
    static void webUIHook(WebUINS::Root &webUI, SensorPlugin::SensorType type);

// ------------------------------------------------------------------------
// MQTT
// ------------------------------------------------------------------------
public:
    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const;
    virtual void onConnect() override;
    virtual void onMessage(const char *topic, const char *payload, size_t len) override;

    void _publishState();

public:
    static void loop();
    static void standbyLoop();

private:
    void _loop();
    void _setupTimer();

    // returns AnimationType::MAX if the name is invalid
    // searched for name, name slug or AnimationType as integer
    // case insensitive
    AnimationType _getAnimationType(const __FlashStringHelper *name) const;
    // get name of the animation
    const __FlashStringHelper *_getAnimationName(AnimationType type) const;
    // get lower case names with dashes
    // "Color Fade" ->  "color-fade"
    const __FlashStringHelper *_getAnimationNameSlug(AnimationType type) const;
    // get title for animation
    const __FlashStringHelper *_getAnimationTitle(AnimationType type) const;

public:
    void enableLoop(bool enable);
    void enableLoopNoClear(bool enable);

    void setColorAndRefresh(Color color);
    // time represents fading level 0 to max, the fading time is relative to the different between the brightness levels
    void setBrightness(uint8_t brightness, int32_t millis = -1, uint32_t maxTime = ~0U);
    // use NONE to remove all animations
    // use NEXT to remove the current animation and start the next one. if next animation isnt set, animation is set to NONE
    void setAnimation(AnimationType animation, uint16_t blendTime = Clock::BlendAnimation::kDefaultTime);

    uint16_t _blendTime{Clock::BlendAnimation::kDefaultTime};

    // read defaults and copy to local storage
    void readConfig(bool setup);
    // get stored configuration and update it with local storage
    ClockConfigType &getWriteableConfig();

    // ------------------------------------------------------------------------
    // Motion sensor
    // ------------------------------------------------------------------------
    #if IOT_SENSOR_HAVE_MOTION_SENSOR
    public:
        virtual void eventMotionDetected(bool motion) override;
        virtual bool eventMotionAutoOff(bool state);

    private:
        bool _motionAutoOff{false};

    #endif

    // ------------------------------------------------------------------------
    // Fan control
    // ------------------------------------------------------------------------
    #ifdef IOT_LED_MATRIX_FAN_CONTROL

    private:
        void _setFanSpeed(uint8_t speed);
        // void _webUIUpdateFanSpeed();

    private:
        uint8_t _fanSpeed{0};

    #endif

    // ------------------------------------------------------------------------
    // Power consumption sensor
    // ------------------------------------------------------------------------
    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION || IOT_CLOCK_HAVE_POWER_LIMIT

    public:
        static uint8_t calcPowerFunction(uint8_t scale, uint32_t data);
        static void webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id);

    private:
        String _getPowerLevelStr();
        void _updatePowerLevelWebUI();
        uint8_t _calcPowerFunction(uint8_t scale, uint32_t data);
        void _powerLevelCallback(uint32_t total_mW, uint32_t requested_mW, uint32_t max_mW, uint8_t target_brightness, uint8_t recommended_brightness);
        void _webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id);
        void _calcPowerLevel();
        float __getPowerLevel(float P_W, float min) const;
        uint32_t _getPowerLevelLimit(uint32_t P_Watt) const;

        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION

            float _getPowerLevel() const {
                if (!_isEnabled || _tempBrightness == -1) {
                    return NAN;
                }
                return __getPowerLevel(_powerLevelAvg / 1000.0, 1.039);
            }

        private:
            static constexpr uint32_t kPowerLevelUpdateRateMultiplier = 500000U;

            float _powerLevelAvg;
            uint32_t _powerLevelUpdateTimer{0};
            uint32_t _powerLevelCurrentmW{0};
            uint32_t _powerLevelUpdateRate{kUpdateMQTTInterval * kPowerLevelUpdateRateMultiplier};
        #endif

    #endif

// ------------------------------------------------------------------------
// Enable/disable LEDs
// ------------------------------------------------------------------------
public:
    static void clear() {
        _reset();
    }

private:
    static void _reset();
    void _enable();
    void _disable();
    bool _getEnabledState() const {
        return _config.enabled && _isEnabled && _targetBrightness && _tempBrightness != -1;
    }

    // ------------------------------------------------------------------------
    // Save state
    // ------------------------------------------------------------------------
    #if IOT_CLOCK_SAVE_STATE

    public:
        void _saveStateDelayed();
        void _saveState();
        StoredState _getState() const;

        Event::Timer _saveTimer;
        uint32_t _saveTimestamp{0};
    #else

    public:
        void _saveStateDelayed() {}
        void _saveState() {}

    #endif

    void _setState(bool state, bool autoOff = false);

    // ------------------------------------------------------------------------
    // Button
    // ------------------------------------------------------------------------
    #if IOT_CLOCK_BUTTON_PIN != -1

    public:
        using EventType = Clock::Button::EventType;
        void buttonCallback(uint8_t button, EventType eventType, uint16_t repeatCount);

        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            void rotaryCallback(bool decrease, uint32_t now);
            void setRotaryAction(uint8_t action);

        private:
            // acceleration per step
            static constexpr int kRotaryAccelerationDivider = 1;
            // max. acceleration
            static constexpr int kRotaryMaxAcceleration = 10 * kRotaryAccelerationDivider;

            uint32_t _lastRotaryUpdate{0};
            uint8_t _rotaryAcceleration{kRotaryAccelerationDivider};
            uint8_t _rotaryAction{0};
            Event::Timer _rotaryActionTimer;
        #endif

    #endif

    // ------------------------------------------------------------------------
    // Alarm Plugin
    // ------------------------------------------------------------------------
    #if IOT_ALARM_PLUGIN_ENABLED

    public:
        using AlarmModeType = KFCConfigurationClasses::Plugins::AlarmConfigNS::ModeType;

        static void alarmCallback(AlarmModeType mode, uint16_t maxDuration);

    private:
        void _alarmCallback(AlarmModeType mode, uint16_t maxDuration);
        bool _resetAlarm();  // returns true if alarm was reset

        Event::Timer _alarmTimer;
        Event::Callback _resetAlarmFunc;
    #endif

    #if !IOT_LED_MATRIX

    // ------------------------------------------------------------------------
    // Clock
    // ------------------------------------------------------------------------

    // private:
    //     void _setSevenSegmentDisplay();

    public:
        void setBlinkColon(uint16_t value);

    private:
        // std::array<SevenSegmentDisplay::PixelAddressType, IOT_CLOCK_PIXEL_ORDER_LEN * IOT_CLOCK_NUM_DIGITS> _pixelOrder;
        time_t _time{0};

    #    if IOT_CLOCK_PIXEL_SYNC_ANIMATION
    public:
        void setSyncing(bool sync);
        static void ntpCallback(time_t now);

    private:
        bool _loopSyncingAnimation(LoopOptionsType &options);

    private:
        bool _isSyncing{false};
    #    endif

    #endif

// ------------------------------------------------------------------------

public:
    void _broadcastWebUI();
    void _webUIUpdateColor(int color = -1);

private:
    bool isTempProtectionActive() const {
        return _tempBrightness == -1;
    }
    float getTempProtectionFactor() const {
        if (isTempProtectionActive()) {
            return 0;
        }
        return _tempBrightness;
    }

public:
    // void setAnimationCallback(Clock::AnimationCallback callback);
    // void setUpdateRate(uint16_t updateRate);
    // uint16_t getUpdateRate() const;
    void setColor(Color color);
    Color getColor() const;

private:
    friend WebServer::AsyncUpdateWebHandler;

    // set brightness
    // enable LEDs if disabled
    // store new level in config
    void _setBrightness(uint8_t brightness, bool useEnable = true);

    // update brightness settings savedBrightmess, config.brightness and config.enabled
    void _updateBrightnessSettings();

    // returns display brightness using current brightness and auto brightness value
    uint8_t _getBrightness(bool temperatureProtection = true) const;

    // returns current brightness without auto brightness value
    // while fading between different levels it returns the current level
    float _getFadingBrightness() const;

    // set current color
    // store color in config as solid_color if no animation is active
    void _setColor(uint32_t color, bool updateAnimation = true);

    // get current color
    uint32_t _getColor() const;

    void _setAnimation(Clock::Animation *animation);
    void _setBlendAnimation(Clock::Animation *animation);

// ------------------------------------------------------------------------
// Singleton getter
// ------------------------------------------------------------------------
public:
    static ClockPlugin &getInstance();

// ------------------------------------------------------------------------
// private variables
// ------------------------------------------------------------------------

private:
    friend Clock::LEDMatrixLoopOptions;
    friend Clock::ClockLoopOptions;
    friend Clock::VisualizerAnimation;

    // SevenSegmentDisplay _display;
    bool _schedulePublishState;
    bool _isFading;
    bool _isEnabled;
    bool _forceUpdate;
    bool _isRunning;

    using Animation = Clock::Animation;
    friend Animation;

    Clock::DisplayType _display;

    Color __color;
    uint32_t _lastUpdateTime;
    uint8_t _tempOverride;
    float _tempBrightness;
    float _fps;
    String _overheatedInfo;

    ClockConfigType _config;
    Event::Timer _timer;
    uint32_t _timerCounter;

    MillisTimer _fadeTimer;
    uint8_t _savedBrightness;
    uint8_t _startBrightness;
    uint8_t _fadingBrightness;
    uint8_t _targetBrightness;

    Clock::Animation *_animation;
    Clock::BlendAnimation *_blendAnimation;
    Clock::ShowMethodType _method;

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2
        AmbientLightSensorHandler _lightSensor2;
    #endif

    #if IOT_LED_MATRIX_HAVE_SSD1306
    private:
        #ifndef SSD1306_DISPLAY_CONFIG
            #error SSD1306_DISPLAY_CONFIG not set
        #endif
        Adafruit_SSD1306 _ssd1306{SSD1306_DISPLAY_CONFIG};
        Event::Timer _ssd1306Timer;
        bool _ssd1306Blank{false};

        void ssd1306Begin();
        void ssd1306End();
        void ssd1306Clear(bool display = true);
        void ssd1306Update();
        // enable screen saver/blank screen
        void ssd1306Blank(bool state);

        static void ssd1306InitTimer(Event::CallbackTimerPtr);
        static void ssd1306UpdateTimer(Event::CallbackTimerPtr);
        static void ssd1306WiFiCallback(WiFiCallbacks::EventType, void *);
    #endif

public:
    static Clock::ShowMethodType getShowMethod();
    static const __FlashStringHelper *getShowMethodStr();
    static void setShowMethod(Clock::ShowMethodType method);
    static void toggleShowMethod();
};

inline void ClockPlugin::setColor(Color color)
{
    _setColor(color);
}

inline ClockPlugin::Color ClockPlugin::getColor() const
{
    return _getColor();
}

inline void ClockPlugin::standbyLoop()
{
    ::delay(50); // energy saving mode
}

inline void ClockPlugin::enableLoop(bool enable)
{
    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        setAutoBrightness(enable ? (Plugins::Sensor::getConfig().ambient.auto_brightness != -1) : false);
    #endif
    _display.clear();
    _display.show();
    enableLoopNoClear(enable);
}

inline void ClockPlugin::enableLoopNoClear(bool enable)
{
    __LDBG_printf("enable loop=%u", enable);
    LoopFunctions::remove(standbyLoop);
    if (enable) {
        LoopFunctions::add(loop);
        _fps = 0;
    }
    else {
        LoopFunctions::remove(loop);
        _fps = NAN;
    }
}

#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION || IOT_CLOCK_HAVE_POWER_LIMIT

inline uint8_t ClockPlugin::calcPowerFunction(uint8_t scale, uint32_t data)
{
    return getInstance()._calcPowerFunction(scale, data);
}

inline void ClockPlugin::webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id)
{
    if (getInstance()._isRunning) {
        getInstance()._webSocketCallback(type, client, server, id);
    }
}

inline float ClockPlugin::__getPowerLevel(float P, float min) const
{
    return std::max<float>(IOT_CLOCK_POWER_CORRECTION_OUTPUT);
}

inline uint32_t ClockPlugin::_getPowerLevelLimit(uint32_t P_Watt) const
{
    if (P_Watt == 0) {
        return ~0U; // unlimited
    }
    return P_Watt * 1090;
    // return std::max<float>(0, IOT_CLOCK_POWER_CORRECTION_LIMIT);
}

#endif

extern ClockPlugin ClockPlugin_plugin;

inline ClockPlugin &ClockPlugin::getInstance()
{
    return ClockPlugin_plugin;
}

#if IOT_ALARM_PLUGIN_ENABLED

inline void ClockPlugin::alarmCallback(AlarmModeType mode, uint16_t maxDuration)
{
    getInstance()._alarmCallback(mode, maxDuration);
}

inline bool ClockPlugin::_resetAlarm()
{
    __LDBG_printf("alarm_func=%u alarm_state=%u", _resetAlarmFunc ? 1 : 0, AlarmPlugin::getAlarmState());
    if (_resetAlarmFunc) {
        // reset prior clock settings
        _resetAlarmFunc(*_timer);
        AlarmPlugin::resetAlarm();
        _schedulePublishState = true;
        return true;
    }
    return false;
}

#endif

#if IOT_SENSOR_HAVE_MOTION_SENSOR && IOT_SENSOR_HAVE_MOTION_AUTO_OFF

inline void ClockPlugin::eventMotionDetected(bool motion)
{
    digitalWrite(131, !motion);
    #if IOT_LED_MATRIX_HAVE_SSD1306
        ssd1306Blank(!motion);
    #endif
}

inline bool ClockPlugin::eventMotionAutoOff(bool state)
{
    // state true = turn off
    if (state && _isEnabled) {
        _setState(false, true);
        return true;
    }
    // state false = turn on
    if (!state && !_isEnabled && _motionAutoOff == true) {
        _setState(true, false);
        return true;
    }
    return false;
}

#endif

#if IOT_CLOCK_PIXEL_SYNC_ANIMATION

inline void ClockPlugin::ntpCallback(time_t now)
{
    getIntance().setSyncing(false);
}

inline void ClockPlugin::setSyncing(bool sync)
{
    __LDBG_printf("sync=%u", sync);
    if (_tempProtection == ProtectionConfigType::MAX) {
        __LDBG_printf("temperature protection active");
        return;
    }
    if (sync != _isSyncing) {
        _isSyncing = sync;
        _time = 0;
        _display.clear();
        _updateRate = 100;
        __LDBG_printf("update_rate=%u", _updateRate);
        _forceUpdate = true;
    }
}
#endif

#if IOT_LED_MATRIX_FAN_CONTROL

inline void ClockPlugin::_setFanSpeed(uint8_t speed)
{
#if DEBUG_IOT_CLOCK
    auto setSpeed = speed;
#endif
    if (speed < _config.min_fan_speed) {
        speed = 0;
    }
    else {
        speed = std::min<uint8_t>(speed, _config.max_fan_speed);
    }
    analogWrite(TINYPWM_BASE_PIN, speed);
    _fanSpeed = speed;
#if DEBUG_IOT_CLOCK
    __DBG_printf("set %u speed %u result %u", setSpeed, speed, _fanSpeed);
#endif
}

#endif

inline Clock::ShowMethodType ClockPlugin::getShowMethod()
{
    return getInstance()._method;
}

inline void ClockPlugin::setShowMethod(Clock::ShowMethodType method)
{
    getInstance()._method = method;
}

inline void ClockPlugin::toggleShowMethod()
{
    constexpr auto kFirst = static_cast<int>(Clock::ShowMethodType::NONE);
    constexpr auto kRange = static_cast<int>(Clock::ShowMethodType::MAX) - kFirst;
    auto method = static_cast<uint8_t>(getInstance()._method);
    method -= kFirst + 1;
    if (method < 0) {
        method = 0;
    }
    else {
        method %= kRange;
    }
    getInstance()._method = static_cast<Clock::ShowMethodType>(method + kFirst);
}


#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR

inline uint8_t ClockPlugin::_getBrightness(bool temperatureProtection) const
{
    return isAutoBrightnessEnabled() ?
        (_getFadingBrightness() * (getAutoBrightness()) * (temperatureProtection ? getTempProtectionFactor() : 1.0f)) :
        (temperatureProtection ?
            (_getFadingBrightness() * getTempProtectionFactor()) :
            _getFadingBrightness());
}

#else

inline uint8_t ClockPlugin::_getBrightness(bool temperatureProtection) const
{
    return temperatureProtection ?
        (_getFadingBrightness() * getTempProtectionFactor()) :
        _getFadingBrightness();
}

#endif

inline float ClockPlugin::_getFadingBrightness() const
{
    return (_fadeTimer.isActive() && _fadeTimer.getDelay()) ?
        (_targetBrightness - (((int16_t)_targetBrightness - (int16_t)_startBrightness) / (float)_fadeTimer.getDelay()  * _fadeTimer.getTimeLeft())) :
        _targetBrightness;
}

inline ClockPlugin::AnimationType ClockPlugin::_getAnimationType(const __FlashStringHelper *name) const
{
    return ClockConfigType::getAnimationType(name);
}

inline const __FlashStringHelper * ClockPlugin::_getAnimationName(AnimationType type) const
{
    return ClockConfigType::getAnimationName(type);
}

inline const __FlashStringHelper * ClockPlugin::_getAnimationNameSlug(AnimationType type) const
{
    return ClockConfigType::getAnimationNameSlug(type);
}

inline const __FlashStringHelper *ClockPlugin::_getAnimationTitle(AnimationType type) const
{
    return ClockConfigType::getAnimationTitle(type);
}

#if !IOT_LED_MATRIX

inline void ClockPlugin::setBlinkColon(uint16_t value)
{
    if (value < kMinBlinkColonSpeed) {
        value = 0;
    }
    _config.blink_colon_speed = value;
    _schedulePublishState = true;
    __LDBG_printf("blinkcolon=%u update_rate=%u", value, value);
}

#endif

inline void ClockPlugin::setColorAndRefresh(Color color)
{
    __LDBG_printf("color=%s", color.toString().c_str());
    _setColor(color);
    _forceUpdate = true;
    _schedulePublishState = true;
}

inline void ClockPlugin::_setColor(uint32_t color, bool updateAnimation)
{
    __color = color;
    if (_config.getAnimation() == AnimationType::SOLID) {
        _config.solid_color = __color;
    }
    #if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
        else if (_config.getAnimation() == AnimationType::VISUALIZER) {
            _config.visualizer.color = __color;
        }
    #endif
    if (updateAnimation && _animation) {
        _animation->setColor(__color);
    }
}

inline uint32_t ClockPlugin::_getColor() const
{
    return __color;
}

inline Clock::ClockConfigType &ClockPlugin::getWriteableConfig()
{
    // get a writeable copy and update with it with the current config
    auto &cfg = Plugins::Clock::getWriteableConfig();
    #if IOT_CLOCK_SAVE_STATE
        if (_saveTimer) {
            // save pending state
            _saveState();
        }
        // check if a config state is stored
        auto state = _getState();
        if (state.hasValidData()) {
            _config = state.getConfig();
        }
    #endif
    cfg = _config;
    return cfg;
}

inline void ClockPlugin::_setAnimation(Clock::Animation *animation)
{
    __LDBG_printf("animation=%p _ani=%p _blend_ani=%p", animation, _animation, _blendAnimation);
    if (_animation) {
        _setBlendAnimation(animation);
    }
    else {
        _animation = animation;
        _animation->begin();
    }
}

inline void ClockPlugin::_setBlendAnimation(Clock::Animation *blendAnimation)
{
    __LDBG_printf("blend=%p", blendAnimation);
    // we do not support blending more than 2 animations
    // if switched to quickly it will stop and continue with the new one
    if (_blendAnimation) {
        delete _blendAnimation;
    }
    _blendAnimation = new Clock::BlendAnimation(_animation, blendAnimation, _display, _blendTime);
    if (!_blendAnimation) {
        delete _animation;
        _animation = blendAnimation;
        return;
    }
    _blendAnimation->begin();
}

inline void ClockPlugin::_updateBrightnessSettings()
{
    if (_targetBrightness != 0) {
        __LDBG_printf("saved=%u set=%u", _savedBrightness, _targetBrightness);
        _savedBrightness = _targetBrightness;
        _config.setBrightness(_targetBrightness);
        // __LDBG_assert_printf(_config.enabled, "_config.enabled not true");
        // _config.enabled = true;
    }
    else {
        // __LDBG_assert_printf(!_config.enabled, "_config.enabled not false");
        // _config.enabled = false;
    }
}

inline void ClockPlugin::_reset()
{
    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, kEnablePinState(true));
    #endif
    NeoPixelEx::forceClear<IOT_LED_MATRIX_OUTPUT_PIN>(IOT_CLOCK_NUM_PIXELS);
    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, kEnablePinState(false));
    #endif
    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, INPUT);
}

inline void ClockPlugin::loop()
{
    getInstance()._loop();
}

inline const __FlashStringHelper *ClockPlugin::getShowMethodStr()
{
    switch(getShowMethod()) {
        case Clock::ShowMethodType::NONE:
            return F("None");
        case Clock::ShowMethodType::FASTLED:
            return F("FastLED");
        case Clock::ShowMethodType::NEOPIXEL:
            return F("NeoPixel");
        case Clock::ShowMethodType::NEOPIXEL_REPEAT:
            return F("NeoPixelRepeat");
        default:
            break;
    }
    return F("Unknown");
}

extern "C" uint8_t getNeopixelShowMethodInt();
extern "C" const __FlashStringHelper *getNeopixelShowMethodStr();

inline const __FlashStringHelper *getNeopixelShowMethodStr()
{
    return ClockPlugin::getShowMethodStr();
}

extern "C" void ClockPluginClearPixels();

inline Clock::LoopOptionsBase::LoopOptionsBase(ClockPlugin &plugin) :
    // _updateRate(plugin._updateRate),
    _forceUpdate(plugin._forceUpdate),
    _brightness(plugin._getBrightness()),
    _millis(millis()),
    _millisSinceLastUpdate(get_time_diff(plugin._lastUpdateTime, _millis))
{
    if (plugin._isFading && plugin._fadeTimer.reached()) {
        __LDBG_printf("fading=done brightness=%u target_brightness=%u", plugin._getBrightness(), plugin._targetBrightness);
        plugin._setBrightness(plugin._targetBrightness);
        plugin._isFading = false;
        // update mqtt and webui
        plugin._publishState();
    }
}

#if IOT_LED_MATRIX == 0

inline Clock::ClockLoopOptions::ClockLoopOptions(ClockPlugin &plugin) :
    LoopOptionsBase(plugin),
    _time(plugin._time),
    _now(time(nullptr)),
    _tm({}),
    format_24h(plugin._config.time_format_24h)
{
}

inline struct Clock::ClockLoopOptions::tm24 &Clock::ClockLoopOptions::getLocalTime(time_t *nowPtr)
{
    if (!nowPtr) {
        nowPtr = &_now;
    }
    _tm = localtime(nowPtr);
    _tm.set_format_24h(format_24h);
    return _tm;
}

#endif

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
