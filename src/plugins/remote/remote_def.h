/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <PinMonitor.h>
#include <stl_ext/fixed_circular_buffer.h>
#include <kfc_fw_config_classes.h>

#ifndef DEBUG_IOT_REMOTE_CONTROL
#define DEBUG_IOT_REMOTE_CONTROL                                0
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

#ifndef IOT_REMOTE_CONTROL_COMBO_BTN
#define IOT_REMOTE_CONTROL_COMBO_BTN                            1
#endif

class HassPlugin;

using namespace PinMonitor;
using KFCConfigurationClasses::Plugins;

namespace RemoteControl {

    static const uint32_t kAutoSleepDisabled = ~0;

    enum class EventType : uint8_t {
        NONE = 0,
        REPEAT,
        PRESS,
        RELEASE,
        COMBO_REPEAT,
        COMBO_RELEASE,
        MAX
    };

    //using ComboActionType = Plugins::RemoteControl::ComboAction_t;
    using MultiClickType = Plugins::RemoteControl::MultiClick_t;
    using ActionType = Plugins::RemoteControl::Action_t;

    class Base;

    class Button : public PushButton {
    public:
        Button();
        Button(uint8_t pin, uint8_t button, Base *base, bool testMode = false);

        uint8_t getNum() const;
        bool isPressed() const;

        virtual void event(EventType state, uint32_t now) override;

        void updateConfig();

        Base *getBase() const {
            return reinterpret_cast<Base *>(const_cast<void *>(getArg()));
        }

    private:
        uint8_t _button;
    };

    class ButtonEvent {
    public:
        static constexpr uint32_t kComboButtonNone = (1 << 3) - 1;

        ButtonEvent();
        ButtonEvent(uint8_t button, EventType type, uint32_t duration = 0, uint32_t repeat = 0, uint32_t comboButton = kComboButtonNone);
        uint8_t getButton() const;
        //int8_t getComboButton() const;
        //ComboActionType getCombo(const ActionType &actions) const;
        EventType getType() const;
        uint16_t getDuration() const;
        uint16_t getRepeat() const;
        uint32_t getTimestamp() const;
        void remove();

#if DEBUG_IOT_REMOTE_CONTROL
        void printTo(Print &output) const;
        String toString() const;
#endif

    private:
        EventType type() const;
        void type(const EventType type);

    private:
        uint32_t _timestamp;
        uint32_t _duration : 14;            // max 16 seconds
        uint32_t _repeat : 9;               // 512x times
        uint32_t _type: 3;
        uint32_t _button: 3;
        uint32_t _comboButton: 3;

        static_assert((int)EventType::MAX < (1U << 3), "too many buttons for _type");
        static_assert(IOT_REMOTE_CONTROL_BUTTON_COUNT < (1U << 3) - 1, "too many buttons for _button");
        static_assert(IOT_REMOTE_CONTROL_BUTTON_COUNT < kComboButtonNone, "too many buttons for _comboButton");
    };

    static constexpr size_t ButtonEventSize = sizeof(ButtonEvent);

    using KFCConfigurationClasses::Plugins;
    using ConfigType = Plugins::RemoteControl::Config_t;
    using ButtonPinsArray = std::array<const uint8_t, IOT_REMOTE_CONTROL_BUTTON_COUNT>;
    using ButtonEventList = stdex::fixed_circular_buffer<ButtonEvent, 32>;

};
