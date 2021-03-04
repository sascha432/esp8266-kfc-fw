/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <PinMonitor.h>
#include <stl_ext/fixed_circular_buffer.h>
#include <kfc_fw_config.h>

// https://www.home-assistant.io/integrations/device_trigger.mqtt/

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

// enable button combinations
#ifndef IOT_REMOTE_CONTROL_COMBO_BTN
#define IOT_REMOTE_CONTROL_COMBO_BTN                            1
#endif

using KFCConfigurationClasses::Plugins;

using namespace PinMonitor;

namespace RemoteControl {

    static const uint32_t kAutoSleepDisabled = ~0; // disable auto sleep

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

    namespace Queue {
        class Event;
        class Target;
    }

    class Button : public PushButton {
    public:
        static const uint32_t kTimeOffsetFirstEvent = ~0;
        static const uint32_t kTimeOffsetUseFirstEvent = 0;

        Button();
        Button(uint8_t pin, uint8_t button, Base *base);

        uint8_t getButtonNum() const;
        bool isPressed() const;

        virtual void event(EventType state, uint32_t now) override;

        void updateConfig();
        Base *getBase() const;

        // static size_t getUdpQueueSize();

        // String actionString(EventType eventType, uint8_t buttonNum, uint16_t repeatCount);

    private:
        // void _sendUdpEvent(EventType type, uint8_t num, uint8_t button, uint32_t micros);
        uint32_t _getEventTimeForFirstEvent();
        uint32_t _getEventTime();

    private:
        uint8_t _button;
        uint32_t _timeOffset;
    };

//     class ButtonEvent {
//     public:
//         static constexpr uint32_t kComboButtonNone = (1 << 3) - 1;

//         ButtonEvent();
//         ButtonEvent(uint8_t button, EventType type, uint32_t duration = 0, uint32_t repeat = 0, uint32_t comboButton = kComboButtonNone);
//         uint8_t getButton() const;
//         //int8_t getComboButton() const;
//         //ComboActionType getCombo(const ActionType &actions) const;
//         EventType getType() const;
//         uint16_t getDuration() const;
//         uint16_t getRepeat() const;
//         uint32_t getTimestamp() const;
//         void remove();

// #if DEBUG_IOT_REMOTE_CONTROL
//         void printTo(Print &output) const;
//         String toString() const;
// #endif

//     private:
//         EventType type() const;
//         void type(const EventType type);

//     private:
//         uint32_t _timestamp;
//         uint32_t _duration : 14;            // max 16 seconds
//         uint32_t _repeat : 9;               // 512x times
//         uint32_t _type: 3;
//         uint32_t _button: 3;
//         uint32_t _comboButton: 3;

//         static_assert((int)EventType::MAX < (1U << 3), "too many buttons for _type");
//         static_assert(IOT_REMOTE_CONTROL_BUTTON_COUNT < (1U << 3) - 1, "too many buttons for _button");
//         static_assert(IOT_REMOTE_CONTROL_BUTTON_COUNT < kComboButtonNone, "too many buttons for _comboButton");
//     };

//     static constexpr size_t ButtonEventSize = sizeof(ButtonEvent);

    using KFCConfigurationClasses::Plugins;
    using ConfigType = Plugins::RemoteControl::Config_t;
    using ButtonPinsArray = std::array<const uint8_t, IOT_REMOTE_CONTROL_BUTTON_COUNT>;
    using EventQueue = std::list<Queue::Event>;
}
