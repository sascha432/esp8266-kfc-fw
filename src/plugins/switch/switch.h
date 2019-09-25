/**
  Author: sascha_lammers@gmx.de
*/

#if IOT_SWITCH

#pragma once

#ifndef DEBUG_IOT_SWITCH
#define DEBUG_IOT_SWITCH                    0
#endif

#include <Arduino_compat.h>
#include <vector>
#include "../mqtt/mqtt_client.h"
#include "pin_monitor.h"
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>

#ifndef IOT_SWITCH_PINS
#error IOT_SWITCH_PINS needs to list button pins for example "#define IOT_SWITCH_PINS D6,D7"
#endif

// active state either LOW or HIGH
#ifndef IOT_SWITCH_ACTIVE_STATE
#define IOT_SWITCH_ACTIVE_STATE             PRESSED_WHEN_LOW
#endif

// initial time to trigger hold event, 0 to disable
#ifndef IOT_SWITCH_HOLD_DURATION
#define IOT_SWITCH_HOLD_DURATION            800
#endif

// trigger hold down event every n milliseconds
#ifndef IOT_SWITCH_HOLD_REPEAT
#define IOT_SWITCH_HOLD_REPEAT              100
#endif

class IOTSwitch : public MQTTComponent {
public:
    class ButtonContainer {
    public:
        ButtonContainer(uint8_t pin) : _pin(pin), _button(pin, IOT_SWITCH_ACTIVE_STATE) {
        }

        inline uint8_t getPin() const {
            return _pin;
        }

        inline PushButton &getButton() {
            return _button;
        }

    private:
        uint8_t _pin;
        PushButton _button;
    };

    typedef std::vector<ButtonContainer> ButtonVector;

    IOTSwitch();
    virtual ~IOTSwitch();

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    static void pinCallback(void *arg);
    static void loop();

    static void onButtonPressed(Button& btn);
    static void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(Button& btn, uint16_t duration);

private:
    ButtonVector _buttons;

    void _pinCallback(PinMonitor::Pin_t &pin);
    void _loop();
};

#endif
