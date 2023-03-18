/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "remote_def.h"
#include "remote_action.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace RemoteControl
{

    namespace Queue {

        class Event;

        //
        // Lock send queue per button or entire device
        //
        class Lock {
        public:
            Lock();

            // lock single button
            void lock(uint8_t buttonNum, ActionPtr action);
            // unlock single button
            void unlock(uint8_t buttonNum);

            // lock all buttons
            void lockAll(ActionPtr action);
            // unlock all buttons except the ones that have a single lock
            void unlockAll();

            // // release lock for action
            // void release(Event &event);

            // remove all locks
            void clear();

            // check if this button is locked
            bool isLocked(uint8_t buttonNum, ActionPtr action) const;

            bool isAnyLocked(ActionPtr action) const;

        private:
            static constexpr uint16_t kAllLockedBit = 0x8000;

            uint16_t _buttons;
            ActionPtr _lockOwner;
        };

        inline Lock::Lock() : _buttons(0), _lockOwner(nullptr)
        {
        }

        inline void Lock::lock(uint8_t buttonNum, ActionPtr action)
        {
            InterruptLock lock;
            // __LDBG_printf("lock=%u owner=%p", buttonNum, action);
            _buttons |= (1 << buttonNum);
        }

        inline void Lock::unlock(uint8_t buttonNum)
        {
            InterruptLock lock;
            // __LDBG_printf("unlock=%u owner=%p", buttonNum, _lockOwner);
            _buttons &= ~(1 << buttonNum);
            _lockOwner = nullptr;
        }

        inline void Lock::lockAll(ActionPtr action)
        {
            InterruptLock lock;
            // __LDBG_printf("lockAll owner=%p", action);
            _buttons |= kAllLockedBit;
        }

        inline void Lock::unlockAll()
        {
            InterruptLock lock;
            // __LDBG_printf("unlockAll owner=%p", _lockOwner);
            _buttons &= ~kAllLockedBit;
            _lockOwner = nullptr;
        }

        inline void Lock::clear()
        {
            InterruptLock lock;
            _buttons = 0;
            _lockOwner = nullptr;
        }

        inline bool Lock::isLocked(uint8_t buttonNum, ActionPtr action) const
        {
            InterruptLock lock;
            if (action == _lockOwner) {
                return false;
            }
            return ((_buttons & kAllLockedBit) || (_buttons & (1 << buttonNum)));
        }

        inline bool Lock::isAnyLocked(ActionPtr action) const
        {
            InterruptLock lock;
            if (action == _lockOwner) {
                return false;
            }
            return (_buttons & kAllLockedBit);
        }


        // // Actions stores data sent to another device
        // //
        // // a button press sends multiple events which can contain different actions
        // //
        // // if the ordered flag is set, events/actions added later are delayed if any error
        // // occurs sending the action. if the events arrive in the correct order also
        // // depends on the underlying protocol
        // //
        // // using tcp with a single connection and/or acknowledgment ensures the order
        // class Actions
        // {
        // public:
        //     enum class OrderType {
        //         NONE = 0,
        //         BUTTON,                 // sending further actions for a single button is delayed until the action has been sent
        //         DEVICE,                 // sending any further actions is delayed until the action has been sent
        //     };

        // public:
        //     Actions(const Actions &) = delete;
        //     Actions &operator=(const Actions &) = delete;

        //     Actions(Actions &&move) noexcept :
        //         _order(move._order),
        //         _udp(std::exchange(move._udp, nullptr)),
        //         _tcp(std::exchange(move._tcp, nullptr)),
        //         _mqtt(std::exchange(move._mqtt, nullptr))
        //     {}

        //     Actions() :
        //         _order(OrderType::NONE),
        //         _udp(nullptr),
        //         _tcp(nullptr),
        //         _mqtt(nullptr)
        //     {}

        //     ~Actions() {
        //         if (_udp) {
        //             delete _udp;
        //         }
        //         if (_tcp) {
        //             delete _tcp;
        //         }
        //         if (_mqtt) {
        //             delete _mqtt;
        //         }
        //     }

        //     Actions &operator=(Actions &&move) noexcept {
        //         this->~Actions();
        //         _order = move._order;
        //         _udp = std::exchange(move._udp, nullptr);
        //         _tcp = std::exchange(move._tcp, nullptr);
        //         _mqtt = std::exchange(move._mqtt, nullptr);
        //         return *this;
        //     }

        //     bool empty() const {
        //         return !_udp && !_tcp && !_mqtt;
        //     }

        //     void setOrder(OrderType type) {
        //         _order = type;
        //     }

        //     OrderType getOrder() const {
        //         return _order;
        //     }

        //     bool send(Event &event, Lock &lock);

        //     template<typename... Args>
        //     void addUdp(Args &&... args) {
        //         _udp = new ActionUDP(std::forward<Args>(args)...);
        //     }

        // private:
        //     OrderType _order;
        //     ActionPtr _udp;
        //     ActionPtr _tcp;
        //     ActionPtr _mqtt;
        // };

        // Event stores one or more actions for a single button
        class Event {
        public:
            enum class LockType {
                NONE = 0,
                BUTTON,
                QUEUE,
            };

            enum class StateType {
                NONE = 0,
                QUEUED,
                SENDING,
                SENT,
                FAILED
            };
        public:

            Event(const Event &) = delete;
            Event &operator=(const Event &) = delete;

            Event() : _type(Button::EventType::NONE), _state(StateType::NONE), _action(nullptr) {}

            Event(Button::EventType type, uint8_t buttonNum, LockType lockType, ActionPtr action) :
                _type(type),
                _state(StateType::QUEUED),
                _buttonNum(buttonNum),
                _lockType(lockType),
                _action(action)
            {}

            Event(Event &&move) :
                _type(std::exchange(move._type, Button::EventType::NONE)),
                _state(std::exchange(move._state, StateType::NONE)),
                _buttonNum(move._buttonNum),
                _lockType(move._lockType),
                _action(std::exchange(move._action, nullptr))
            {}

            Event &operator=(Event &&move) {
                this->~Event();
                _type = std::exchange(move._type, Button::EventType::NONE);
                _state = std::exchange(move._state, StateType::NONE);
                _buttonNum = move._buttonNum;
                _lockType = move._lockType;
                _action = std::exchange(move._action, nullptr);
                return *this;
            }
            ~Event() {
                // __LDBG_printf("~action=%p", _action);
            }

            inline bool operator==(const Event &event) const {
                return this == std::addressof(event);
            }

            // send all actions
            // returns true if it has been sent to all targets
            void send(Lock &lockObj);

            // apply lock
            void lock(Lock &lockObj);

            // apply lock
            void unlock(Lock &lockObj);

            bool canSend() const;
            bool canDelete() const;
            StateType getState() const;
            void setStateType(StateType state);
            uint8_t getButtonNum() const;
            LockType getLockType() const;
            void setAction(ActionPtr action);
            Action &getAction();

        private:
            Button::EventType _type;
            StateType _state;
            uint8_t _buttonNum;
            LockType _lockType;
            ActionPtr _action;
        };

        inline bool Event::canSend() const
        {
            return (_state == StateType::QUEUED) && _action->canSend();
        }

        inline bool Event::canDelete() const
        {
            return (_state != StateType::QUEUED) && (_state != StateType::SENDING);
        }

        inline Event::StateType Event::getState() const
        {
            return _state;
        }

        inline void Event::setStateType(StateType state)
        {
            _state = state;
        }

        inline uint8_t Event::getButtonNum() const
        {
            return _buttonNum;
        }

        inline Event::LockType Event::getLockType() const
        {
            return _lockType;
        }

        inline void Event::setAction(ActionPtr action)
        {
            _action = action;
        }

        inline Action &Event::getAction()
        {
            return *_action;
        }


        // inline void Lock::release(Event &event)
        // {
        //     if (_lockOwner == &event.getAction()) {
        //         switch(event.getLockType()) {
        //             case Event::LockType::BUTTON:
        //                 unlock(event.getButtonNum());
        //                 break;
        //             case Event::LockType::QUEUE:
        //                 unlockAll();
        //                 break;
        //             case Event::LockType::NONE:
        //                 break;
        //         }
        //         _lockOwner = nullptr;
        //     }
        // }

    };
}

#include <debug_helper_disable.h>
