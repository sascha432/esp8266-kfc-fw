/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SWITCH

#include "switch.h"
#include "PluginComponent.h"
#include "plugins.h"
#include <LoopFunctions.h>

IOTSwitch *iotSwitch;
static uint8_t pins[] = { IOT_SWITCH_PINS, 0 };

IOTSwitch::IOTSwitch() : MQTTComponent(SWITCH) {
    auto &monitor = *PinMonitor::createInstance();
    for(uint8_t i = 0; pins[i]; i++) {
        PinMonitor::Pin_t *pin;
        //pinMode(pins[i], INPUT);
        if ((pin = monitor.addPin(pins[i], pinCallback, this))) {
            _buttons.emplace_back(ButtonContainer(pin->pin));
            auto &button = _buttons.back().getButton();
            button.onPress(IOTSwitch::onButtonPressed);
#if IOT_SWITCH_HOLD_DURATION
            button.onHoldRepeat(IOT_SWITCH_HOLD_DURATION, IOT_SWITCH_HOLD_REPEAT, IOTSwitch::onButtonHeld);
#endif
            button.onRelease(IOTSwitch::onButtonReleased);
        }
    }
}

IOTSwitch::~IOTSwitch() {
    auto monitor = PinMonitor::getInstance();
    if (monitor) {
        for(const auto &button: _buttons) {
            monitor->removePin(button.getPin(), this);
        }
        if (monitor->size() == 0) {
            PinMonitor::deleteInstance();
        }
    }
    LoopFunctions::remove(IOTSwitch::loop);
}

void IOTSwitch::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {

    String topic = MQTTClient::formatTopic(-1, F("/"));

    auto discovery = _debug_new MQTTAutoDiscovery();

    discovery->create(this, 0, MQTTAutoDiscovery::FORMAT_JSON);
    discovery->addStateTopic(topic);
    discovery->finalize();
    vector.emplace_back(MQTTComponent::MQTTAutoDiscoveryPtr(discovery));
}

uint8_t IOTSwitch::getAutoDiscoveryCount() const {
    return 1;
}

void IOTSwitch::pinCallback(void *arg) {
    auto pin = reinterpret_cast<PinMonitor::Pin_t *>(arg);
    reinterpret_cast<IOTSwitch *>(pin->arg)->_pinCallback(*pin);
}

void IOTSwitch::loop() {
    iotSwitch->_loop();
}

void IOTSwitch::onButtonPressed(Button& btn) {
    _debug_printf_P(PSTR("onButtonPressed %p\n"), &btn);
}

void IOTSwitch::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount) {
    _debug_printf_P(PSTR("onButtonHeld %p duration %u repeat %u\n"), &btn, duration, repeatCount);
}

void IOTSwitch::onButtonReleased(Button& btn, uint16_t duration) {
    _debug_printf_P(PSTR("onButtonReleased %p duration %u\n"), &btn, duration);
    uint8_t pressed = 0;
    for(auto &button: iotSwitch->_buttons) {
        auto &pushButton = button.getButton();
        if (pushButton.isPressed()) {
            pressed++;
        }
    }
    if (pressed == 0) {
        LoopFunctions::remove(IOTSwitch::loop);
    }
}

void IOTSwitch::_pinCallback(PinMonitor::Pin_t &pin) {
    LoopFunctions::add(IOTSwitch::loop);
    for(auto &button: _buttons) {
        if (button.getPin() == pin.pin) {
            button.getButton().update();
            // _debug_printf_P(PSTR("IOTSwitch::_pinCallback(): pin %d button isPressed() %d\n"), button.getButton().isPressed());
        }
    }
}

void IOTSwitch::_loop() {
    for(auto &button: _buttons) {
        auto &pushButton = button.getButton();
        pushButton.update();
    }
}

class SwitchPlugin : public PluginComponent {
public:
    SwitchPlugin();
    virtual PGM_P getName() const;
    virtual PluginPriorityEnum_t getSetupPriority() const override;
    virtual void setup(PluginSetupMode_t mode) override;
};

static SwitchPlugin plugin;

SwitchPlugin::SwitchPlugin() {
    register_plugin(this);
}

PGM_P SwitchPlugin::getName() const {
    return PSTR("switch");
}

SwitchPlugin::PluginPriorityEnum_t SwitchPlugin::getSetupPriority() const {
    return (PluginPriorityEnum_t)0;
}

void SwitchPlugin::setup(PluginSetupMode_t mode) {
    iotSwitch = new IOTSwitch();
}

#endif
