/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "remote_base.h"
#include "remote_event_queue.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;


namespace RemoteControl {

    // 0 {button_name}-down
    // 1 {button_name}-up
    // 2 {button_name}-press
    // 3 {button_name}-long-press
    // 4 {button_name}-single-click
    // 5 {button_name}-double-click
    // 6 {button_name}-multi-{repeat}-click
    // 7 {button_name}-hold
    // 8 {button_name}-hold-release

    static uint8_t buttonEventToEventNum(Button::EventType type)
    {
        switch(type) {
            case Button::EventType::DOWN:
                return 0;
            case Button::EventType::UP:
                return 1;
            case Button::EventType::PRESSED:
                return 2;
            case Button::EventType::LONG_PRESSED:
                return 3;
            case Button::EventType::SINGLE_CLICK:
                return 4;
            case Button::EventType::DOUBLE_CLICK:
                return 5;
            case Button::EventType::REPEATED_CLICK:
                return 6;
            case Button::EventType::HELD:
                return 7;
            //TODO
            // case Button::EventType::HOLD_RELEASE:
            //     return 8;
            default:
                break;
        }
        return 0;
    }

    void Base::queueEvent(Button::EventType type, uint8_t buttonNum, uint16_t eventCount, uint32_t eventTime, uint16_t actionId)
    {
        auto eventNum = buttonEventToEventNum(type);
        if (_getConfig().events[eventNum].enabled) { // global enable flag

            _getConfig().actions[buttonNum].getUdpBits();

        }



//         if (!skip) {
//             auto action = Button::actionString(type, buttonNum, eventCount);
//             if (action) {
//                 PrintString json(F("{\"device\":\"%s\",\"action\":\"%s\",\"event\":\"%s\",\"button\":%u,\"repeat\":%u,\"ts\":%u}"), System::Device::getName(), action, Button::eventTypeToString(type), buttonNum, eventCount, eventTime);
// #if DEBUG_IOT_REMOTE_CONTROL
//                 String jsonCopy = json;
// #endif
//                 auto action = new ActionUDP(0, Payload::Json(std::move(json)), Plugins::RemoteControl::getUdpHost(), IPAddress(), _getConfig().udp_port);
//                 __LDBG_printf("json=%s", jsonCopy.c_str());
//                 _queue.emplace_back(type, buttonNum, Queue::Event::LockType::NONE, action);
//             }
//         }

        // TODO add actions to target

        //auto event = Queue::Event(type, buttonNum, eventCount, eventTime, _pressed, target);

        if (_queue.size() && !_queueTimer) {
            _queueTimer.add(25, true, [this](Event::CallbackTimerPtr timer) {
                timerCallback(timer);
            });
        }

    }

    void Base::timerCallback(Event::CallbackTimerPtr timer)
    {
        if (_lock.isAnyLocked(nullptr)) {
            return;
        }
        for(auto &item: _queue) {
            if (item.canSend()) {
                if (!_lock.isLocked(item.getButtonNum(), &item.getAction())) {
                    item.send(_lock);
                    if (_lock.isAnyLocked(nullptr)) {
                        return;
                    }
                }
            }
        }

#if DEBUG_IOT_REMOTE_CONTROL && 0
        auto count = _queue.size();
#endif
        noInterrupts();
        _queue.erase(std::remove_if(_queue.begin(), _queue.end(), [](const Queue::Event &event) {
            return event.canDelete();
        }), _queue.end());
        interrupts();
#if DEBUG_IOT_REMOTE_CONTROL && 0
        if (count != _queue.size()) {
            __DBG_printf("%u items removed from queue", count- _queue.size());
        }
#endif

        if (_queue.empty()) {
            timer->disarm();
        }
    }

}
