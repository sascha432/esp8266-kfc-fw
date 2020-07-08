/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>
#include <vector>
#include "plugins.h"
#include "pin_monitor.h"
#include "kfc_fw_config.h"

#ifndef DEBUG_IOT_BUTTONS
#define DEBUG_IOT_BUTTONS                               1
#endif

#define MAX_BUTTONS                                     8

#if !defined(PIN_MONITOR) || !PIN_MONITOR
#error requires PIN_MONITOR=1
#endif

class ButtonsPlugin : public PluginComponent {
public:
    typedef Config_Button::Button_t Button_t;

    class Button {
    public:
        Button(const Button_t &config) : _config(config), _button(PushButton(_config.pin, _config.pinMode)) {
        }
        ~Button() {
        }
        uint8_t getPin() const  {
            return _config.pin;
        }
        uint8_t getPinMode() const  {
            return _config.pinMode;
        }
        Button_t &getConfig() {
            return _config;
        }
        PushButton &getButton() {
            return _button;
        }
    private:
        Button_t _config;
        PushButton _button;
    };

    typedef std::vector<Button> ButtonVector;

public:
    ButtonsPlugin() {
        REGISTER_PLUGIN(this);
    }

    virtual PGM_P getName() const {
        return PSTR("buttons");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Buttons");
    }

    virtual PluginPriorityEnum_t getSetupPriority() const {
        return PRIO_BUTTONS;
    }
    virtual bool autoSetupAfterDeepSleep() const override {
        return true;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void shutdown() override;
    virtual void reconfigure(PGM_P source) override;

    // virtual MenuTypeEnum_t getMenuType() const override {
    //     return AUTO;
    // }
    // virtual void createMenu() override {
    //     bootstrapMenu.addSubMenu(F("Buttons"), F("buttons.html"), navMenu.config);
    // }

    virtual PGM_P getConfigureForm() const override {
        return getName();
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;


public:
    static void pinCallback(void *arg);
    static void loop();

    static void onButtonPressed(::Button& btn);
    static void onButtonHeld(::Button& btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(::Button& btn, uint16_t duration);

private:
    void _readConfig();

private:
    ButtonVector _buttons;
};
