/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "remote_event_queue.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace RemoteControl {

    namespace Queue {

        void Event::lock(Lock &lockObj) {
            switch(_lockType) {
                case LockType::BUTTON:
                    lockObj.lock(_buttonNum, _action);
                    break;
                case LockType::QUEUE:
                    lockObj.lockAll(_action);
                    break;
                case LockType::NONE:
                    break;
            }
        }

        void Event::unlock(Lock &lockObj) {
            switch(_lockType) {
                case LockType::BUTTON:
                    lockObj.unlock(_buttonNum);
                    break;
                case LockType::QUEUE:
                    lockObj.unlockAll();
                    break;
                case LockType::NONE:
                    break;
            }
        }

        void Event::send(Lock &lockObj)
        {
            __LDBG_printf("executing action=%p", _action);
            lock(lockObj);
            _action->execute([this, &lockObj](bool status) {
                __LDBG_printf("action=%p callback status=%u", _action, status);
                if (status) {
                    _state = StateType::SENT;
                }
                else {
                    _state = StateType::FAILED;
                }
                unlock(lockObj);
                delete _action;
                _action = nullptr;
            });
        }

    }

}
