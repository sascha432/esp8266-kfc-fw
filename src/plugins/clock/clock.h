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

#if DEBUG_IOT_CLOCK
#define IOT_CLOCK_MIN_TEMPERATURE_THRESHOLD     20
#else
#define IOT_CLOCK_MIN_TEMPERATURE_THRESHOLD     45
#endif

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
    void setColorAndRefresh(Color color);
    void setBrightness(uint16_t brightness);
    // use NONE to remove all animations
    // use NEXT to remove the current animation and start the next one. if next animation isnt set, animation is set to NONE
    void setAnimation(AnimationType animation);
    void readConfig();

private:
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
    uint16_t _readLightSensor(uint8_t num, uint8_t delayMillis) const;
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
    uint32_t _updateTimer;
    time_t _time;
    uint16_t _updateRate;
    uint8_t _isSyncing : 1;
    uint8_t _schedulePublishState : 1;
    uint8_t _displaySensorValue : 1;
    uint8_t _forceUpdate: 1;
    uint8_t _tempProtection : 2;
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    int16_t _autoBrightness;
    float _autoBrightnessValue;
    uint8_t _autoBrightnessLastValue;
    EventScheduler::Timer _autoBrightnessTimer;
#endif
    Clock_t _config;
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
