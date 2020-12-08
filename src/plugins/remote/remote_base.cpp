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
using UdpActionType = KFCConfigurationClasses::Plugins::RemoteControl::UdpActionType;
using KFCConfigurationClasses::Plugins;

namespace RemoteControl {

    void Base::queueEvent(Button::EventType type, uint8_t buttonNum, uint16_t eventCount, uint32_t eventTime, uint16_t actionId)
    {
        auto actions = _getConfig()._get_enum_udpAction();
        bool skip = true;
        switch(actions) {
            case UdpActionType::INCLUDE_UP_DOWN:
                skip = false;
                break;
            case UdpActionType::EXCLUDE_UP_DOWN:
                skip = (type == Button::EventType::DOWN || type == Button::EventType::UP);
                break;
            default:
            case UdpActionType::NONE:
                skip = true;
                break;
        }
        if (!skip) {
            auto action = Button::actionString(type, buttonNum, eventCount);
            if (action) {
                PrintString json(F("{\"device\":\"%s\",\"action\":\"%s\",\"event\":\"%s\",\"button\":%u,\"repeat\":%u,\"ts\":%u}"), System::Device::getName(), action, Button::eventTypeToString(type), buttonNum, eventCount, eventTime);
#if DEBUG_IOT_REMOTE_CONTROL
                String jsonCopy = json;
#endif
                auto action = new ActionUDP(0, Payload::Json(std::move(json)), Plugins::RemoteControl::getUdpHost(), IPAddress(), _getConfig().udpPort);
                __LDBG_printf("json=%s", jsonCopy.c_str());
                _queue.emplace_back(type, buttonNum, Queue::Event::LockType::NONE, action);
            }
        }

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
