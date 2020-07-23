/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
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
#define IOT_CLOCK_BUTTON_PIN                    14
#endif

#if IOT_CLOCK_BUTTON_PIN
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>
#endif

class ClockPlugin : public PluginComponent, public MQTTComponent {
public:
    using SevenSegmentDisplay = Clock::SevenSegmentDisplay;
    using Color = Clock::Color;

    static constexpr uint16_t kDefaultUpdateRate = 1000;
    static constexpr uint16_t kMinBlinkColonSpeed = 50;
    static constexpr uint16_t kMinFlashingSpeed = 50;
    static constexpr uint16_t kMinRainbowSpeed = 1;

private:
    uint8_t _ui_colon;
    uint8_t _ui_animation;
    uint8_t _ui_color;

// PluginComponent
public:
    typedef std::function<void()> UpdateCallback_t;

    typedef enum : uint8_t {
        NONE = 0,
        RAINBOW,
        FLASHING,
        FADE,
        MAX
    } AnimationEnum_t;

    ClockPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
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
    virtual uint8_t getAutoDiscoveryCount() const {
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
        return 2;
#else
        return 1;
#endif
    }
    virtual void onConnect(MQTTClient *client);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

    void publishState(MQTTClient *client = nullptr);

private:
    void _setColor(Color color);

private:
    uint8_t _colors[3];
    uint16_t _brightness;

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
    using Alarm = KFCConfigurationClasses::Plugins::Alarm;

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
    void setAnimation(AnimationEnum_t animation);
    void readConfig();

private:
    void _loop();
    void _setSevenSegmentDisplay();
    void _setBrightness();
    uint16_t _getBrightness() const;
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    void _adjustAutobrightness();
    void _updateLightSensorWebUI();
#endif

private:
    static constexpr int16_t AUTO_BRIGHTNESS_OFF = -1;

#if IOT_CLOCK_BUTTON_PIN
    PushButton _button;
    uint8_t _buttonCounter;
#endif

    SevenSegmentDisplay _display;
    std::array<SevenSegmentDisplay::pixel_address_t, IOT_CLOCK_PIXEL_ORDER_LEN * IOT_CLOCK_NUM_DIGITS> _pixelOrder;

    Color _color;
    Color _savedColor;
    uint32_t _updateTimer;
    time_t _time;
    uint16_t _updateRate;
    uint8_t _isSyncing;
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    int16_t _autoBrightness;
    float _autoBrightnessValue;
    uint8_t _autoBrightnessLastValue;
    EventScheduler::Timer _autoBrightnessTimer;
#endif
    Clock_t _config;
    EventScheduler::Timer _timer;
    uint32_t _timerCounter;
    bool _tempProtection;
private:
    friend Clock::Animation;
    friend Clock::FadingAnimation;
    friend Clock::RainbowAnimation;
    friend Clock::FlashingAnimation;

    void setUpdateRate(uint16_t updateRate) {
        _updateRate = updateRate;
        _updateTimer = 0;
    }
    void setColor(Color color) {
        _color = color;
    }
    Color getColor() const {
        return _color;
    }
    void _setAnimation(Clock::Animation *animation);
    void _setNextAnimation(Clock::Animation *animation);
    void _deleteAnimaton();
private:
    Clock::Animation *_animation;
    Clock::Animation *_nextAnimation;
};
