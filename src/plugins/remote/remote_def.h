/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <PinMonitor.h>
#include <stl_ext/fixed_circular_buffer.h>
#include <kfc_fw_config.h>

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
#define  IOT_REMOTE_CONTROL_BUTTON_PINS                         13,12,14,4
// #define IOT_REMOTE_CONTROL_BUTTON_PINS                          14, 4, 12, 13   // D5, D2, D6, D7
#endif

// set high while running
// the default using GPIO15 which has a pulldown to enable energy saving mode during deep sleep
#ifndef IOT_REMOTE_CONTROL_AWAKE_PIN
#define IOT_REMOTE_CONTROL_AWAKE_PIN                            15
#endif

// pin for charging detection (active high)
#ifndef IOT_REMOTE_CONTROL_CHARGING_PIN
#define IOT_REMOTE_CONTROL_CHARGING_PIN                         5
#endif

// enable button combinations
#ifndef IOT_REMOTE_CONTROL_COMBO_BTN
#define IOT_REMOTE_CONTROL_COMBO_BTN                            1
#endif

#if !PIN_MONITOR_USE_GPIO_INTERRUPT
#error PIN_MONITOR_USE_GPIO_INTERRUPT=1 required
#endif

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
    using MultiClickType = KFCConfigurationClasses::Plugins::RemoteControl::MultiClick_t;
    using ActionType = KFCConfigurationClasses::Plugins::RemoteControl::Action_t;

    class Base;

    enum class BaseEventType : uint8_t {
        NONE,
        CHARGE_DETECTION
    };

    namespace Queue {
        class Event;
        class Target;
    }

    class ChargingDetection : public Pin {
    public:
        ChargingDetection(uint8_t pin, Base *base) : Pin(pin, base, StateType::UP_DOWN, ActiveStateType::ACTIVE_HIGH) {}

        virtual void event(StateType state, uint32_t now) override;

    private:
        Base &getBase() const {
            return *reinterpret_cast<Base *>(const_cast<void *>(getArg()));
        }
    };

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

    static constexpr auto kButtonPins = stdex::cexpr::array_of<uint8_t>(IOT_REMOTE_CONTROL_BUTTON_PINS);

    using KFCConfigurationClasses::Plugins;
    using ConfigType = Plugins::RemoteControl::Config_t;
    using ButtonPinsArray = decltype(kButtonPins);
    using EventQueue = std::list<Queue::Event>;
}
