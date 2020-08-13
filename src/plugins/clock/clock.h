/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <vector>
#include "animation.h"
#include "WebUIComponent.h"
#include "plugins.h"
#include "kfc_fw_config.h"
#include "./plugins/mqtt/mqtt_component.h"
#if IOT_ALARM_PLUGIN_ENABLED
#include "./plugins/alarm/alarm.h"
#endif

#if SPEED_BOOSTER_ENABLED
#error The speed booster causes timing issues and should be deactivated
#endif

#if !NTP_CLIENT || !NTP_HAVE_CALLBACKS
#error NTP_CLIENT=1 and NTP_HAVE_CALLBACKS=1 required
#endif

#ifndef IOT_CLOCK_BUTTON_PIN
#define IOT_CLOCK_BUTTON_PIN                            14
#endif

#if IOT_CLOCK_BUTTON_PIN
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>
#endif

#if DEBUG_IOT_CLOCK
#define IOT_CLOCK_MIN_TEMPERATURE_THRESHOLD             20
#else
#define IOT_CLOCK_MIN_TEMPERATURE_THRESHOLD             45
#endif

// number of measurements. 0=disable
#ifndef IOT_CLOCK_DEBUG_ANIMATION_TIME
// #define IOT_CLOCK_DEBUG_ANIMATION_TIME                  0
#define IOT_CLOCK_DEBUG_ANIMATION_TIME                  50
#endif

#if IOT_CLOCK_DEBUG_ANIMATION_TIME
#define __DBGTM(...) __VA_ARGS__
#include <FixedCircularBuffer.h>
extern FixedCircularBuffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _anmationTime;
extern FixedCircularBuffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _animationRenderTime;
extern FixedCircularBuffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _displayTime;
extern FixedCircularBuffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _timerDiff;
#else
#define __DBGTM(...)
#endif

using KFCConfigurationClasses::Plugins;

class ClockPlugin : public PluginComponent, public MQTTComponent {
public:
    using SevenSegmentDisplay = Clock::SevenSegmentDisplay;
    using Color = Clock::Color;
    using AnimationType = Clock::AnimationType;

    static constexpr uint16_t kDefaultUpdateRate = 1000;
    static constexpr uint16_t kMinBlinkColonSpeed = 50;
    static constexpr uint16_t kMinFlashingSpeed = 50;
    static constexpr uint16_t kMinRainbowSpeed = 1;

    static constexpr uint8_t kUpdateAutobrightnessInterval = 2;
    static constexpr uint8_t kCheckTemperatureInterval = 10;
    static constexpr uint8_t kUpdateMQTTInterval = 30;

    // restore brightness if temperature is kTemperatureRestoreDifference below any threshold
    static constexpr uint8_t kTemperatureThresholdDifference = 10;
    static constexpr uint8_t kMinimumTemperatureThreshold = IOT_CLOCK_MIN_TEMPERATURE_THRESHOLD;

    static constexpr int16_t kAutoBrightnessOff = -1;

    enum ProtectionType {
        OFF,
        B75,
        B50,
        MAX
    };
    static_assert(ProtectionType::MAX <= 3, "stored _tempProtection / 2 bit");

// PluginComponent
public:

    ClockPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

// WebUI
public:
    virtual void createWebUI(WebUI &webUI) override;
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

// MQTT
public:
    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const;
    virtual void onConnect(MQTTClient *client);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

    void _publishState(MQTTClient *client = nullptr);

public:
#if IOT_CLOCK_BUTTON_PIN
    static void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(Button& btn, uint16_t duration);
    void _onButtonReleased(uint16_t duration);
#endif
    static void loop();
    static void ntpCallback(time_t now);

#if IOT_ALARM_PLUGIN_ENABLED
public:
    using Alarm = Plugins::Alarm;

    static void alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration);

private:
    void _alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration);
    bool _resetAlarm(); // returns true if alarm was reset

    EventScheduler::Timer _alarmTimer;
    EventScheduler::Callback _resetAlarmFunc;
#endif

public:
    void enable(bool enable);
    void setSyncing(bool sync);
    void setBlinkColon(uint16_t value);
    void setColorAndRefresh(Color color);
    void setBrightness(uint16_t brightness);
    // use NONE to remove all animations
    // use NEXT to remove the current animation and start the next one. if next animation isnt set, animation is set to NONE
    void setAnimation(AnimationType animation);
    void readConfig();

protected:
    using ConfigStructType = Plugins::Clock::ConfigStructType;

    class LoopOptionsType
    {
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
    bool _loopUpdateButtons(LoopOptionsType &options);
    bool _loopSyncingAnimation(LoopOptionsType &options);
    bool _loopDisplayLightSensor(LoopOptionsType &options);
    void _loop();

    void _setSevenSegmentDisplay();
    void _setBrightness();
    void _setBrightness(uint16_t brightness);
    uint16_t _getBrightness() const;
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    void _installWebHandlers();
    void _adjustAutobrightness();
    String _getLightSensorWebUIValue();
    void _updateLightSensorWebUI();
    uint16_t _readLightSensor() const;
    // uint16_t _readLightSensor(uint8_t num, uint8_t delayMillis) const;
    void _broadcastWebUI();
public:
    static void handleWebServer(AsyncWebServerRequest *request);
#endif

private:


#if IOT_CLOCK_BUTTON_PIN
    PushButton _button;
    uint8_t _buttonCounter;
#endif

    SevenSegmentDisplay _display;
    std::array<SevenSegmentDisplay::PixelAddressType, IOT_CLOCK_PIXEL_ORDER_LEN * IOT_CLOCK_NUM_DIGITS> _pixelOrder;

    uint16_t _brightness;
    uint16_t _savedBrightness;
    Color _color;
    uint32_t _lastUpdateTime;
    time_t _time;
    uint16_t _updateRate;
    uint8_t _isSyncing : 1;
    uint8_t _displaySensorValue : 2;
    uint8_t _tempProtection : 2;
    // keep aligned and 32bit to avoid locking interrupts when reading/writing
    volatile bool _schedulePublishState;
    volatile bool _forceUpdate;
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    int16_t _autoBrightness;
    float _autoBrightnessValue;
    uint8_t _autoBrightnessLastValue;
    EventScheduler::Timer _autoBrightnessTimer;
#endif
    Plugins::Clock::ConfigStructType _config;
    EventScheduler::Timer _timer;
    uint32_t _timerCounter;

private:
    bool _isTemperatureBelowThresholds(float temp) const;
    void _startTempProtectionAnimation();

public:
    void setAnimationCallback(Clock::AnimationCallback callback);
    void setUpdateRate(uint16_t updateRate);
    uint16_t getUpdateRate() const;
    void setColor(Color color);
    Color getColor() const;

private:
    void _setAnimation(Clock::Animation *animation);
    void _setNextAnimation(Clock::Animation *animation);
    void _deleteAnimaton(bool startNext);
private:
    Clock::Animation *_animation;
    Clock::Animation *_nextAnimation;
};
