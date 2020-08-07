/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Trailing edge dimmer module
// I2C or UART interface
// 1 - 8 channels
// https://github.com/sascha432/trailing_edge_dimmer
//
// default I2C pins are D3 (0) and D5 (14)
// NOTE: I2C Wire.onReceive() is not working on ESP8266

#include <Arduino_compat.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include <array>
#include <kfc_fw_config.h>
#include "./plugins/mqtt/mqtt_client.h"
#include "serial_handler.h"
#include "dimmer_module_form.h"
#include "dimmer_channel.h"
#include "dimmer_base.h"
#include "pin_monitor.h"

// number of channels
#ifndef IOT_DIMMER_MODULE_CHANNELS
#define IOT_DIMMER_MODULE_CHANNELS          1
#endif

// use UART instead of I2C
#ifndef IOT_DIMMER_MODULE_INTERFACE_UART
#define IOT_DIMMER_MODULE_INTERFACE_UART    0
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART

// UART only change baud rate of the Serial port to match the dimmer module
#ifndef IOT_DIMMER_MODULE_BAUD_RATE
#define IOT_DIMMER_MODULE_BAUD_RATE         57600
#endif

#else

// I2C only. SDA PIN
#ifndef IOT_DIMMER_MODULE_INTERFACE_SDA
#define IOT_DIMMER_MODULE_INTERFACE_SDA     D3
#endif

// I2C only. SCL PIN
#ifndef IOT_DIMMER_MODULE_INTERFACE_SCL
#define IOT_DIMMER_MODULE_INTERFACE_SCL     D5
#endif

#endif

// max. brightness level
#ifndef IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#define IOT_DIMMER_MODULE_MAX_BRIGHTNESS    16666
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART
#include "SerialTwoWire.h"
#else
#include <Wire.h>
#endif

// enable or disable buttons
#ifndef IOT_DIMMER_MODULE_HAS_BUTTONS
#define IOT_DIMMER_MODULE_HAS_BUTTONS       0
#endif

#if IOT_DIMMER_MODULE_HAS_BUTTONS
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>
#endif

#if IOT_DIMMER_MODULE_HAS_BUTTONS && !PIN_MONITOR
#error PIN_MONITOR=1 required
#endif

// pins for dimmer buttons
// each channel needs 2 buttons, up & down
#ifndef IOT_DIMMER_MODULE_BUTTONS_PINS
#define IOT_DIMMER_MODULE_BUTTONS_PINS      D6, D7
#endif

#ifndef IOT_DIMMER_MODULE_PINMODE
#define IOT_DIMMER_MODULE_PINMODE           INPUT
#endif

#ifndef IOT_SWITCH_ACTIVE_STATE
#define IOT_SWITCH_ACTIVE_STATE             PRESSED_WHEN_LOW
#endif

class DimmerModuleForm;

class Driver_DimmerModule: public MQTTComponent, public Dimmer_Base, public DimmerModuleForm
{
public:
    Driver_DimmerModule();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override {
        return nullptr;
    }
    virtual uint8_t getAutoDiscoveryCount() const override {
        return 0;
    }
    virtual void onConnect(MQTTClient *client) override;

    virtual bool on(uint8_t channel = -1) override;
    virtual bool off(uint8_t channel = -1) override;
    virtual int16_t getChannel(uint8_t channel) const override;
    virtual bool getChannelState(uint8_t channel) const override;
    virtual void setChannel(uint8_t channel, int16_t level, float time = -1) override;
    virtual uint8_t getChannelCount() const override {
        return _channels.size();
    }

    virtual void _onReceive(size_t length) override;

protected:
    void _begin();
    void _end();
    void _beginMqtt();
    void _endMqtt();
    void _beginButtons();
    void _endButtons();
    void _printStatus(Print &out);

private:
    void _getChannels();

protected:
    std::array<DimmerChannel, IOT_DIMMER_MODULE_CHANNELS> _channels;

// buttons
#if IOT_DIMMER_MODULE_HAS_BUTTONS
public:
    static void pinCallback(void *arg);
    static void loop();

    static void onButtonPressed(Button& btn);
    static void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(Button& btn, uint16_t duration);

private:
    void _pinCallback(PinMonitor::Pin &pin);
    void _loop();

    void _buttonShortPress(uint8_t channel, bool up);
    void _buttonLongPress(uint8_t channel, bool up);
    void _buttonRepeat(uint8_t channel, bool up, uint16_t repeatCount);

    // get number of pressed buttons, channel and up or down button. returns false if no match
    bool _findButton(Button &btn, uint8_t &pressed, uint8_t &channel, bool &buttonUp);

private:
    inline DimmerModuleButtons getButtonConfig() {
        return config._H_GET(Config().dimmer_buttons);
    }

    class DimmerButton {
    public:
        DimmerButton(uint8_t pin) : _pin(pin), _button(pin, IOT_SWITCH_ACTIVE_STATE) {
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

    typedef std::vector<DimmerButton> DimmerButtonVector;

    DimmerButtonVector _buttons;
    std::array<EventScheduler::Timer, IOT_DIMMER_MODULE_CHANNELS> _turnOffTimer;
    std::array<uint8_t, IOT_DIMMER_MODULE_CHANNELS> _turnOffTimerRepeat;
    std::array<int16_t, IOT_DIMMER_MODULE_CHANNELS> _turnOffLevel;
#endif
};

class DimmerModulePlugin : public PluginComponent, public Driver_DimmerModule {
public:
    DimmerModulePlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createWebUI(WebUI &webUI) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override {
        if (isCreateFormCallbackType(type)) {
            readConfig();
            DimmerModuleForm::_createConfigureForm(type, formName, form, request);
        }
    }

    virtual void getValues(JsonArray &array) override {
        _getValues(array);
    }
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override {
        _setValue(id, value, hasValue, state, hasState);
    }

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override {
        _atModeHelpGenerator(getName_P());
    }
    virtual bool atModeHandler(AtModeArgs &args) override {
        return _atModeHandler(args, *this, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
    }
#endif
};

extern DimmerModulePlugin dimmer_plugin;
