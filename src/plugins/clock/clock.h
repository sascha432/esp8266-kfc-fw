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
#if IOT_ALARM_PLUGIN_ENABLED
#    include "../src/plugins/alarm/alarm.h"
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
#    include <Bounce2.h>
#    include <Button.h>
#    include <ButtonEventCallback.h>
#    include <PushButton.h>
#endif

// number of measurements. 0=disable
#ifndef IOT_CLOCK_DEBUG_ANIMATION_TIME
#    define IOT_CLOCK_DEBUG_ANIMATION_TIME 0
// #define IOT_CLOCK_DEBUG_ANIMATION_TIME                  50
#endif

#if IOT_CLOCK_DEBUG_ANIMATION_TIME
#    define __DBGTM(...) __VA_ARGS__
#    include <stl_ext/fixed_circular_buffer.h>
extern stdex::fixed_circular_buffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _anmationTime;
extern stdex::fixed_circular_buffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _animationRenderTime;
extern stdex::fixed_circular_buffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _displayTime;
extern stdex::fixed_circular_buffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _timerDiff;
#else
#    define __DBGTM(...)
#endif

using KFCConfigurationClasses::Plugins;

#define CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES

class ClockPlugin : public PluginComponent, public MQTTComponent {
public:
    using SevenSegmentDisplay = Clock::SevenSegmentDisplay;
    using Color               = Clock::Color;
    using AnimationType       = Clock::AnimationType;
    using InitialStateType    = Clock::InitialStateType;
    using BrightnessType      = Clock::BrightnessType;
    using StoredState         = Clock::StoredState;
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

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
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

public:
    void enableLoop(bool enable);

    void setColorAndRefresh(Color color);
    // time represents fading level 0 to max, the fading time is relative to the different between the brightness levels
    void setBrightness(BrightnessType brightness, int32_t millis = -1);
    // use NONE to remove all animations
    // use NEXT to remove the current animation and start the next one. if next animation isnt set, animation is set to NONE
    void setAnimation(AnimationType animation);

    // read defaults and copy to local storage
    void readConfig();
    // get stored configuration and update it with local storage
    Clock::Config_t &getWriteableConfig();

// ------------------------------------------------------------------------
// Enable/disable LED pin
// ------------------------------------------------------------------------
#if IOT_CLOCK_HAVE_ENABLE_PIN

#    define __TMP CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    undef CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    define CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES __TMP _isEnabled(false),
#    undef __TMP

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

#    define __TMP CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    undef CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    define CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES __TMP _saveTimestamp(0),
#    undef __TMP

public:
    void _saveStateDelayed();
    void _saveState(int32_t brightness = -1);
    StoredState _getState() const;
    void _copyToState(StoredState &state, BrightnessType brightness, Color color);
    void _copyFromState(const StoredState &state);

    Event::Timer _saveTimer;
    uint32_t _saveTimestamp{0};
#endif
    void _setState(bool state);
    // ------------------------------------------------------------------------
    // Button
    // ------------------------------------------------------------------------

#if IOT_CLOCK_BUTTON_PIN

#    define __TMP CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    undef CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    define CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES __TMP _buttonCounter(0),
#    undef __TMP

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

#if IOT_LED_MATRIX
    // ------------------------------------------------------------------------
    // LED Matrix
    // ------------------------------------------------------------------------

private:
    friend LEDMatrix::LoopOptionsType;
    using LoopOptionsType = LEDMatrix::LoopOptionsType;

#else
    // ------------------------------------------------------------------------
    // Clock
    // ------------------------------------------------------------------------

private:
    class LoopOptionsType {
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
        LoopOptionsType(ClockPlugin &plugin);

        // time since the display was updated
        uint32_t getTimeSinceLastUpdate() const;
        // returns the value of millis() when the object was created
        uint32_t getMillis() const;
        // returns the value of time(nullptr) when the object was created
        time_t getNow() const;
        // return localtime struct tm which provides tm_hour_format() returning the hour in 12 or 24h format
        struct tm24 &getLocalTime(time_t *nowPtr = nullptr);
        // returns true if _updateRate milliseconds have been passed since last update
        bool doUpdate() const;
        // time has changed by one second since last update
        bool hasTimeChanged() const;
        // returns true that the display needs to be redrawn
        // call once per object instance/loop, a second call returns false
        bool doRedraw();

 private:
        ClockPlugin &_plugin;
        uint32_t _millis;
        uint32_t _millisSinceLastUpdate;
        time_t _now;
        struct tm24 _tm;
    };

private:
    void _setSevenSegmentDisplay();

public:
    void setBlinkColon(uint16_t value);

#    define __TMP CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    undef CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    define CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES __TMP _time(0), _isSyncing(false), _displaySensor(DisplaySensorType::OFF),
#    undef __TMP

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

#    define __TMP CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    undef CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES
#    define CLOCK_PLUG_CONSTRUCTOR_INIT_VALUES __TMP _autoBrightnessValue(1023), _autoBrightness(0), _autoBrightnessLastValue(0),
#    undef __TMP


private:
    float _autoBrightnessValue{1023.0};
    Event::Timer _autoBrightnessTimer;
    int16_t _autoBrightness{0};
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
    void setAnimationCallback(Clock::AnimationCallback callback);
    void setUpdateRate(uint16_t updateRate);
    uint16_t getUpdateRate() const;
    void setColor(Color color);
    Color getColor() const;

private:
    // set brightness
    // enable LEDs if disabled
    // store new level in config
    void _setBrightness(BrightnessType brightness);

    // returns display brightness using current brightness and auto brightness value
    BrightnessType _getBrightness(bool temperatureProtection = true) const;

    // returns current brightness without auto brightness value
    // while fading between different levels it returns the current level
    BrightnessType _getFadingBrightness() const;

    // set current color
    // store color in config as solid_color if no animation is active
    void _setColor(uint32_t color);

    // get current color
    uint32_t _getColor() const;

    void _setAnimation(Clock::Animation *animation);
    void _setNextAnimation(Clock::Animation *animation);
    void _deleteAnimaton(bool startNext);
    void _setAnimatonNone();

    // ------------------------------------------------------------------------
    // private variables
    // ------------------------------------------------------------------------

private:
    SevenSegmentDisplay _display;
    bool _schedulePublishState : 1;
    bool _forceUpdate : 1;
    bool _isFading : 1;

    Color __color;
    uint32_t _lastUpdateTime;
    uint16_t _updateRate;
    uint8_t _tempOverride;
    float _tempBrightness;

    Plugins::Clock::ConfigStructType _config;
    Event::Timer _timer;
    uint32_t _timerCounter;

    MillisTimer _fadeTimer;
    BrightnessType _savedBrightness;
    BrightnessType _startBrightness;
    BrightnessType _targetBrightness;

    Clock::Animation *_animation;
    Clock::Animation *_nextAnimation;
};
