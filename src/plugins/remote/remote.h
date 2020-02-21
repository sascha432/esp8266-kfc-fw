/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

// TODO
// add mqtt switch
// form to configure auto sleep delay, button settings etc..
// button settings, hold time, repeat time, double click?
// button action, mqtt, http postback

#if IOT_REMOTE_CONTROL

#include <EventScheduler.h>
#include <EventTimer.h>
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>
#include <list>
#include "../mqtt/mqtt_client.h"
#include  "plugins.h"

#ifndef DEBUG_IOT_REMOTE_CONTROL
#define DEBUG_IOT_REMOTE_CONTROL                                1
#endif

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// number of buttons available
#ifndef IOT_REMOTE_CONTROL_BUTTON_COUNT
#define IOT_REMOTE_CONTROL_BUTTON_COUNT                         4
#endif

// button pins
#ifndef IOT_REMOTE_CONTROL_BUTTON_PINS
#define IOT_REMOTE_CONTROL_BUTTON_PINS                          { 14, 4, 12, 13 }   // D5, D2, D6, D7
#endif

// set high while running
#ifndef IOT_REMOTE_CONTROL_AWAKE_PIN
#define IOT_REMOTE_CONTROL_AWAKE_PIN                            15
#endif

// signal led
#ifndef IOT_REMOTE_CONTROL_LED_PIN
#define IOT_REMOTE_CONTROL_LED_PIN                              2
#endif

class HassPlugin;

class RemoteControlPlugin : public PluginComponent {
public:
    static const uint32_t AutoSleepDisabled = ~0;

    class ButtonEvent {
    public:
        typedef enum {
            NONE = 0,
            REPEAT,
            PRESS,
            RELEASE,
        } EventTypeEnum_t;

        ButtonEvent(uint8_t button, EventTypeEnum_t type, uint16_t duration = 0, uint16_t repeat = 0) : _button(button), _type(type), _duration(duration), _repeat(repeat) {
            _timestamp = millis();
        }

        uint8_t getButton() const {
            return _button;
        }

        EventTypeEnum_t getType() const {
            return _type;
        }

        uint16_t getDuration() const {
            return _duration;
        }

        uint16_t getRepeat() const {
            return _repeat;
        }

        uint32_t getTimestamp() const {
            return _timestamp;
        }

        void remove() {
            _type = NONE;
        }

        void printTo(Print &output) const {
            output.printf_P(PSTR("%u="), _button);
            switch(_type) {
                case PRESS:
                    output.printf_P(PSTR("press,d=%u"), _duration);
                    break;
                case RELEASE:
                    output.printf_P(PSTR("release,d=%u"), _duration);
                    break;
                case REPEAT:
                    output.printf_P(PSTR("repeat,d=%u,c=%u"), _duration, _repeat);
                    break;
                case NONE:
                    output.printf_P(PSTR("none"));
                    break;
            }
            output.printf_P(PSTR(",ts=%u"), _timestamp);
        }

        String toString() const {
            PrintString str;
            printTo(str);
            return str;
        }

    private:
        uint8_t _button;
        EventTypeEnum_t _type;
        uint16_t _duration;
        uint16_t _repeat;
        uint32_t _timestamp;
    };

    typedef std::list<ButtonEvent> ButtonEventList;

public:
    RemoteControlPlugin();

    virtual PGM_P getName() const {
        return PSTR("remotectrl");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Remote Control");
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
        return plugin->nameEquals(FSPGM(http));
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual MenuTypeEnum_t getMenuType() const override {
        return CUSTOM;
    }
    virtual void createMenu() override;

    virtual PGM_P getConfigureForm() const override {
        return getName();
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void onButtonPressed(Button& btn);
    static void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(Button& btn, uint16_t duration);

    static void loop();
    static void wifiCallback(uint8_t event, void *payload);

    static void disableAutoSleep();
    static void disableAutoSleepHandler(AsyncWebServerRequest *request);
    static void deepSleepHandler(AsyncWebServerRequest *request);

private:
    void _loop();
    void _wifiConnected();
    int8_t _getButtonNum(Button &btn) const;
    bool _isUsbPowered() const;
    void _readConfig();
    void _installWebhooks();
    void _resetAutoSleep();
    void _addButtonEvent(const ButtonEvent &event);
    void _sendEvents();
    void _scheduleSendEvents();

    bool _isButtonLocked(uint8_t button) const {
        return _buttonsLocked & (1 << button);
    }
    void _lockButton(uint8_t button) {
        _debug_printf_P(PSTR("btn %u locked\n"), button);
        _buttonsLocked |= (1 << button);
    }
    void _unlockButton(uint8_t button) {
        _debug_printf_P(PSTR("btn %u unlocked\n"), button);
        _buttonsLocked &= ~(1 << button);
    }

    uint32_t _autoSleepTimeout;
    PushButton *_buttons[IOT_REMOTE_CONTROL_BUTTON_COUNT];
    uint32_t _buttonsLocked;
    ButtonEventList _events;
    Config_RemoteControl _config;
    HassPlugin &_hass;

    const uint8_t _buttonPins[IOT_REMOTE_CONTROL_BUTTON_COUNT] = IOT_REMOTE_CONTROL_BUTTON_PINS;
};

#include <debug_helper_disable.h>

#endif
