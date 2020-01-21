/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if IOT_REMOTE_CONTROL

#include <EventScheduler.h>
#include <EventTimer.h>
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>
#include "../mqtt/mqtt_client.h"
#include  "plugins.h"

#ifndef DEBUG_IOT_REMOTE_CONTROL
#define DEBUG_IOT_REMOTE_CONTROL                                1
#endif

// number of buttons available
#ifndef IOT_REMOTE_CONTROL_BUTTON_COUNT
#define IOT_REMOTE_CONTROL_BUTTON_COUNT                         4
#endif

// button pins
#ifndef IOT_REMOTE_CONTROL_BUTTON_PINS
#define IOT_REMOTE_CONTROL_BUTTON_PINS                          { 14, 12, 13, 4 }
#endif

// set high while running
#ifndef IOT_REMOTE_CONTROL_AWAKE_PIN
#define IOT_REMOTE_CONTROL_AWAKE_PIN                            15
#endif

// signal led
#ifndef IOT_REMOTE_CONTROL_LED_PIN
#define IOT_REMOTE_CONTROL_LED_PIN                              2
#endif

class RemoteControlPlugin : public PluginComponent, public MQTTComponent {
public:
    RemoteControlPlugin();

    virtual PGM_P getName() const {
        return PSTR("remotectrl");
    }

    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return MAX_PRIORITY;
    }

    virtual bool autoSetupAfterDeepSleep() const override {
        return true;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;
    virtual void prepareDeepSleep(uint32_t sleepTimeMillis);
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const {
        return plugin->nameEquals(F("http"));
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual MenuTypeEnum_t getMenuType() const override {
        return CUSTOM;
    }
    virtual void createMenu() override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, AtModeArgs &args) override;
#endif

public:
    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector);
    virtual uint8_t getAutoDiscoveryCount() const {
        return 0;//IOT_REMOTE_CONTROL_BUTTON_COUNT;
    }

    virtual void onConnect(MQTTClient *client) override;
    // virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason);
    // virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

public:
    static void onButtonPressed(Button& btn);
    static void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(Button& btn, uint16_t duration);

    static void loop();
    static void timerCallback(EventScheduler::TimerPtr);
    static void wifiCallback(uint8_t event, void *payload);

    static void disableAutoSleep();
    static void disableAutoSleepHandler(AsyncWebServerRequest *request);

private:
    void _loop();
    void _timerCallback();
    void _wifiConnected();
    int8_t _getButtonNum(Button &btn) const;
    bool _isUsbPowered() const;
    void _installWebhooks();
    void _resetAutoSleep();

    uint8_t _autoSleepTime;
    uint32_t _autoSleepTimeout;
    EventScheduler::Timer _timer;
    PushButton *_buttons[IOT_REMOTE_CONTROL_BUTTON_COUNT];

    const uint8_t _buttonPins[IOT_REMOTE_CONTROL_BUTTON_COUNT] = IOT_REMOTE_CONTROL_BUTTON_PINS;
};

#endif

