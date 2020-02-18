/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_MODULE

// Trailing edge dimmer module
// I2C or UART interface
// 1 - 8 channels
// https://github.com/sascha432/trailing_edge_dimmer
//
// default I2C pins are D3 (0) and D5 (14)
// NOTE: Wire.onReceive() is not working on ESP8266

#include <Arduino_compat.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include "kfc_fw_config.h"
#include "../mqtt/mqtt_client.h"
#include "serial_handler.h"
#include "dimmer_module_form.h"
#include "dimmer_channel.h"
#include "dimmer_base.h"
#include "pin_monitor.h"
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>

#ifndef DEBUG_IOT_DIMMER_MODULE
#define DEBUG_IOT_DIMMER_MODULE             0
#endif

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

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;

    virtual bool on(uint8_t channel = -1) override;
    virtual bool off(uint8_t channel = -1) override;
    virtual int16_t getChannel(uint8_t channel) const override;
    virtual bool getChannelState(uint8_t channel) const override;
    virtual void setChannel(uint8_t channel, int16_t level, float time = -1) override;
    virtual uint8_t getChannelCount() const override;

    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override {
        Dimmer_Base::readConfig();
        DimmerModuleForm::createConfigureForm(request, form);
    }

protected:
    void _begin();
    void _end();
    void _printStatus(Print &out);

private:
    void _getChannels();

    DimmerChannel _channels[IOT_DIMMER_MODULE_CHANNELS];

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
    EventScheduler::Timer _turnOffTimer[IOT_DIMMER_MODULE_CHANNELS];
    uint8_t _turnOffTimerRepeat[IOT_DIMMER_MODULE_CHANNELS];
    int16_t _turnOffLevel[IOT_DIMMER_MODULE_CHANNELS];
#endif
};

class DimmerModulePlugin : public Driver_DimmerModule {
public:
    DimmerModulePlugin() : Driver_DimmerModule() {
        REGISTER_PLUGIN(this);
    }

    virtual PGM_P getName() const override;
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Dimmer");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)100;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override;
    virtual void restart() override;

    virtual bool hasStatus() const override;
    virtual void getStatus(Print &output) override;

    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override;

#if AT_MODE_SUPPORTED //&& !IOT_DIMMER_MODULE_INTERFACE_UART
    virtual bool hasAtMode() const override;
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

extern DimmerModulePlugin dimmer_plugin;

#endif
