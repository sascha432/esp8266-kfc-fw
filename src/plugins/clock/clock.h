/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <MillisTimer.h>
#include <vector>
#include "../src/plugins/mqtt/mqtt_component.h"
#include "WebUIComponent.h"
#include "animation.h"
#include "kfc_fw_config.h"
#include "plugins.h"
#include "stored_state.h"
#include "../src/plugins/sensor/sensor.h"
#if IOT_ALARM_PLUGIN_ENABLED
#    include "../src/plugins/alarm/alarm.h"
#endif
#if HTTP2SERIAL_SUPPORT
#include "../src/plugins/http2serial/http2serial.h"
#endif
#if IOT_LED_MATRIX
#    include "led_matrix.h"
#endif

#if SPEED_BOOSTER_ENABLED
#    error The speed booster causes timing issues and should be deactivated
#endif

#if !NTP_CLIENT || !NTP_HAVE_CALLBACKS
#    error NTP_CLIENT=1 and NTP_HAVE_CALLBACKS=1 required
#endif

#ifndef IOT_CLOCK_BUTTON_PIN

#    define IOT_CLOCK_BUTTON_PIN 14
#    ifndef IOT_CLOCK_SAVE_STATE
#        define IOT_CLOCK_SAVE_STATE 1
#    endif

#else

#    ifndef IOT_CLOCK_SAVE_STATE
#        define IOT_CLOCK_SAVE_STATE 0
#    endif

#endif

#ifndef IOT_CLOCK_SAVE_STATE_DELAY
// delay in seconds before any changes get stored except for power on/off
#    define IOT_CLOCK_SAVE_STATE_DELAY 30
#endif

// pin to enable to disable all LEDs
#ifndef IOT_CLOCK_HAVE_ENABLE_PIN
#    define IOT_CLOCK_HAVE_ENABLE_PIN 0
#endif

// pin to enable LEDs
#ifndef IOT_CLOCK_EN_PIN
#    define IOT_CLOCK_EN_PIN 15
#endif

// IOT_CLOCK_EN_PIN_INVERTED=1 sets IOT_CLOCK_EN_PIN to active low
#ifndef IOT_CLOCK_EN_PIN_INVERTED
#    define IOT_CLOCK_EN_PIN_INVERTED 0
#endif

// disable ambient light sensor by default
#ifndef IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#    define IOT_CLOCK_AMBIENT_LIGHT_SENSOR 0
#endif

// add sensor for calculated power level
#ifndef IOT_CLOCK_DISPLAY_CALC_POWER_CONSUMPTION
#   define IOT_CLOCK_DISPLAY_CALC_POWER_CONSUMPTION 0
#endif

#ifndef IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE
#   define IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE 2
#endif

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#    ifndef IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
// update interval in ms, 0 to disable
#        define IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL 125
#    endif
#endif

// show rotating animation while time is invalid
#ifndef IOT_CLOCK_PIXEL_SYNC_ANIMATION
#    define IOT_CLOCK_PIXEL_SYNC_ANIMATION 0
#endif
#if IOT_LED_MATRIX && IOT_CLOCK_PIXEL_SYNC_ANIMATION
#    error not supported, set IOT_CLOCK_PIXEL_SYNC_ANIMATION=0
#endif

#if IOT_CLOCK_BUTTON_PIN
#include <PinMonitor.h>
#endif

using KFCConfigurationClasses::Plugins;

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
        bool doRefresh() const;
        bool doRedraw();
        time_t getNow() const;

    protected:
        // uint16_t _updateRate;
        bool &_forceUpdate;
        uint8_t _brightness;
        bool _doRefresh;
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
        return (_millisSinceLastUpdate >= Clock::kUpdateRate);
    }

    inline bool LoopOptionsBase::doRefresh() const
    {
        return _forceUpdate || _doRefresh;
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

class ClockPlugin : public PluginComponent, public MQTTComponent {
public:
    // using SevenSegmentDisplay = Clock::SevenSegmentDisplay;
    using Color               = Clock::Color;
    using AnimationType       = Clock::AnimationType;
    using InitialStateType    = Clock::InitialStateType;
    using StoredState         = Clock::StoredState;
    using LoopOptionsType     = Clock::LoopOptionsType;
    using milliseconds        = std::chrono::duration<uint32_t, std::ratio<1>>;
    using seconds             = std::chrono::duration<uint32_t, std::ratio<1000>>;

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
        return (active ?
#if IOT_CLOCK_EN_PIN_INVERTED
                       LOW : HIGH
#else
                       HIGH : LOW
#endif
        );
    }

    enum class DisplaySensorType : uint8_t {
        OFF  = 0,
        SHOW = 1,
        BUSY = 2  // waiting for ADC results
    };

    // ------------------------------------------------------------------------
    // PluginComponent
    // ------------------------------------------------------------------------
public:
    ClockPlugin();

#if IOT_CLOCK_DISPLAY_CALC_POWER_CONSUMPTION
    virtual void preSetup(SetupModeType mode) override;
#endif
    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED

private:
#if HTTP2SERIAL_SUPPORT && IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
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
    virtual void createWebUI(WebUIRoot &webUI) override;
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

    // ------------------------------------------------------------------------
    // MQTT
    // ------------------------------------------------------------------------
public:
    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const;
    virtual void onConnect(MQTTClient *client);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

    void _publishState(MQTTClient *client = nullptr);

public:
    static void loop();

private:
    void _loop();
    void _setupTimer();

    // returns AnimationType::MAX if the name is invalid
    AnimationType _getAnimationType(String name);

public:
    void enableLoop(bool enable);

    void setColorAndRefresh(Color color);
    // time represents fading level 0 to max, the fading time is relative to the different between the brightness levels
    void setBrightness(uint8_t brightness, int32_t millis = -1);
    // use NONE to remove all animations
    // use NEXT to remove the current animation and start the next one. if next animation isnt set, animation is set to NONE
    void setAnimation(AnimationType animation);

    // read defaults and copy to local storage
    void readConfig();
    // get stored configuration and update it with local storage
    Clock::Config_t &getWriteableConfig();


// ------------------------------------------------------------------------
// Power consumption sensor
// ------------------------------------------------------------------------

#if IOT_CLOCK_DISPLAY_CALC_POWER_CONSUMPTION

public:
    static void addPowerSensor(WebUIRoot &webUI, WebUIRow **row, SensorPlugin::SensorType type);

private:
    void _updatePowerLevelWebUI();
    void _powerLevelCallback(uint32_t total_mW, uint32_t requested_mW, uint32_t max_mW, uint8_t target_brightness, uint8_t recommended_brightness);

#endif

// ------------------------------------------------------------------------
// Enable/disable LED pin
// ------------------------------------------------------------------------
#if IOT_CLOCK_HAVE_ENABLE_PIN

private:
    void _enable();
    void _disable();

private:
    bool _isEnabled{false};
#endif

    // ------------------------------------------------------------------------
    // Save state
    // ------------------------------------------------------------------------

#if IOT_CLOCK_SAVE_STATE

public:
    void _saveStateDelayed(uint8_t brightness);
    void _saveState(uint8_t brightness);
    StoredState _getState() const;

    Event::Timer _saveTimer;
    uint32_t _saveTimestamp{0};
#endif
    void _setState(bool state);
    // ------------------------------------------------------------------------
    // Button
    // ------------------------------------------------------------------------

#if IOT_CLOCK_BUTTON_PIN

public:
    static void onButtonHeld(Button &btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(Button &btn, uint16_t duration);
    void _onButtonReleased(uint16_t duration);

private:
    bool _loopUpdateButtons(LoopOptionsType &options);

private:
    PushButton _button;
    uint8_t _buttonCounter;
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

private:
    void _setSevenSegmentDisplay();

public:
    void setBlinkColon(uint16_t value);

private:
    std::array<SevenSegmentDisplay::PixelAddressType, IOT_CLOCK_PIXEL_ORDER_LEN * IOT_CLOCK_NUM_DIGITS> _pixelOrder;
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
    // Light sensor
    // ------------------------------------------------------------------------

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
private:
    void _adjustAutobrightness();
    String _getLightSensorWebUIValue();
    void _updateLightSensorWebUI();
    uint16_t _readLightSensor() const;
    // uint16_t _readLightSensor(uint8_t num, uint8_t delayMillis) const;
    bool _loopDisplayLightSensor(LoopOptionsType &options);
    void _installWebHandlers();

public:
    static void adjustAutobrightness(Event::CallbackTimerPtr timer);

private:
    float _autoBrightnessValue{1.0};
    Event::Timer _autoBrightnessTimer;
    int16_t _autoBrightness{ADCManager::kMaxADCValue};
    uint8_t _autoBrightnessLastValue{0};
    DisplaySensorType _displaySensor{DisplaySensorType::OFF};
#endif

    // ------------------------------------------------------------------------

public:
    void _broadcastWebUI();
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
    // set brightness
    // enable LEDs if disabled
    // store new level in config
    void _setBrightness(uint8_t brightness);

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
    // void _deleteAnimaton();
    // void _setAnimatonNone();

    // ------------------------------------------------------------------------
    // private variables
    // ------------------------------------------------------------------------

private:
    friend Clock::LEDMatrixLoopOptions;
    friend Clock::ClockLoopOptions;

    // SevenSegmentDisplay _display;
    bool _schedulePublishState : 1;
    bool _isFading : 1;
    bool _forceUpdate;

    using Animation = Clock::Animation;
    friend Animation;

    Clock::DisplayType _display;

    Color __color;
    uint32_t _lastUpdateTime;
    uint8_t _tempOverride;
    float _tempBrightness;

    Clock::Config_t _config;
    Event::Timer _timer;
    uint32_t _timerCounter;

    MillisTimer _fadeTimer;
    uint8_t _savedBrightness;
    uint8_t _startBrightness;
    uint8_t _fadingBrightness;
    uint8_t _targetBrightness;

    Clock::Animation *_animation;
    Clock::BlendAnimation *_blendAnimation;
};

// inline void ClockPlugin::setAnimationCallback(Clock::AnimationCallback callback)
// {
//     // _display.setCallback(callback);
// }

// inline void ClockPlugin::setUpdateRate(uint16_t updateRate)
// {
//     // _updateRate = updateRate;
//     _forceUpdate = true;
// }

// inline uint16_t ClockPlugin::getUpdateRate() const
// {
//     return Clock::kUpdateRate; //_updateRate;
// }

inline void ClockPlugin::setColor(Color color)
{
    _setColor(color);
}

inline ClockPlugin::Color ClockPlugin::getColor() const
{
    return _getColor();
}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
