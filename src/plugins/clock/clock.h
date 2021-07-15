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

using KFCConfigurationClasses::Plugins;

namespace WebServer {
    class AsyncUpdateWebHandler;
}
class ClockPlugin;

namespace Clock {

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
    // using SevenSegmentDisplay = Clock::SevenSegmentDisplay;
    using Color               = Clock::Color;
    using AnimationType       = Clock::AnimationType;
    using InitialStateType    = Clock::InitialStateType;
    using StoredState         = Clock::StoredState;
    using LoopOptionsType     = Clock::LoopOptionsType;
    using milliseconds        = std::chrono::duration<uint32_t, std::ratio<1>>;
    using seconds             = std::chrono::duration<uint32_t, std::ratio<1000>>;
    using NamedArray          = PluginComponents::NamedArray;

    static constexpr uint16_t kDefaultUpdateRate  = 1000;  // milliseconds
    static constexpr uint16_t kMinBlinkColonSpeed = 50;
    static constexpr uint16_t kMinFlashingSpeed   = 50;
    static constexpr uint16_t kMinRainbowSpeed    = 1;

    static constexpr uint8_t kUpdateAutobrightnessInterval = 2;  // seconds
    static constexpr uint8_t kCheckTemperatureInterval     = 5;  // seconds
    static constexpr uint8_t kMinimumTemperatureThreshold  = 30;  // °C
    static constexpr uint8_t kUpdateMQTTInterval           = 30;  // seconds

    static constexpr uint8_t kMaxBrightness = Clock::kMaxBrightness;

    static constexpr int16_t kAutoBrightnessOff = -1;
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    static constexpr float kAutoBrightnessInterval = IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL;  // milliseconds
#endif

    constexpr static uint8_t enablePinState(bool active) {
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
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

// ------------------------------------------------------------------------
// WebUI
// ------------------------------------------------------------------------
public:
    virtual void createWebUI(WebUINS::Root &webUI) override {}
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

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
    AnimationType _getAnimationType(String name);

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
    void readConfig();
    // get stored configuration and update it with local storage
    Clock::ConfigType &getWriteableConfig();

// ------------------------------------------------------------------------
// Motion sensor
// ------------------------------------------------------------------------
#if IOT_SENSOR_HAVE_MOTION_SENSOR
public:
    virtual void eventMotionDetected(bool motion) override;
    virtual bool eventMotionAutoOff(bool off);

private:
    bool _autoOff{false};

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
    void _disable(uint8_t delayMillis = 10);
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
    void _setState(bool state);

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
    using Alarm = Plugins::Alarm;

    static void alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration);

private:
    void _alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration);
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
// Ambient light sensor
// ------------------------------------------------------------------------
#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
    virtual bool isAutobrightnessEnabled() const override {
        return AmbientLightSensorHandler::isAutobrightnessEnabled() && _config.auto_brightness != -1;
    }

#endif

// ------------------------------------------------------------------------

public:
    void _broadcastWebUI();
    void _webUIUpdateColor(int color = -1);
    static void handleWebServer(AsyncWebServerRequest *request);

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

    // SevenSegmentDisplay _display;
    bool _schedulePublishState;
    bool _isFading;
    bool _isEnabled;
    bool _forceUpdate;

    using Animation = Clock::Animation;
    friend Animation;

    Clock::DisplayType _display;

    Color __color;
    uint32_t _lastUpdateTime;
    uint8_t _tempOverride;
    float _tempBrightness;
    String _overheatedInfo;

    Clock::ConfigType _config;
    Event::Timer _timer;
    uint32_t _timerCounter;

    MillisTimer _fadeTimer;
    uint8_t _savedBrightness;
    uint8_t _startBrightness;
    uint8_t _fadingBrightness;
    uint8_t _targetBrightness;

    Clock::Animation *_animation;
    Clock::BlendAnimation *_blendAnimation;
    bool _running;
    Clock::ShowMethodType _method;

public:
    static Clock::ShowMethodType getShowMethod();
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

extern "C" uint8_t getNeopixelShowMethodInt();
extern "C" const __FlashStringHelper *getNeopixelShowMethodStr();

inline Clock::ShowMethodType getNeopixelShowMethod()
{
    return static_cast<Clock::ShowMethodType>(getNeopixelShowMethodInt());
}

inline const __FlashStringHelper *getNeopixelShowMethodStr()
{
    switch(getNeopixelShowMethod()) {
        case Clock::ShowMethodType::FASTLED:
            return F("FastLED");
        case Clock::ShowMethodType::NEOPIXEL:
            return F("NeoPixel");
        default:
            break;
    }
    return F("Unknown");
}

extern "C" void ClockPluginClearPixels();

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
