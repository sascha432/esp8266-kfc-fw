/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include "EventScheduler.h"

#if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1

#if DEBUG_IOT_CLOCK || 1
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace Clock;

void ClockPlugin::beginIRReceiver()
{
    if (_irReceiver) {
        endIRReceiver();
    }
    _irReceiver = new IRrecv(IOT_LED_MATRIX_IR_REMOTE_PIN);
    __LDBG_printf("begin v" _IRREMOTEESP8266_VERSION_STR);
    // _irReceiver->setTolerance(25);
    _irReceiver->enableIRIn();
    _Timer(_irTimer).add(Event::milliseconds(150), true, [this](Event::CallbackTimerPtr) {
        if (_irReceiver->decode(&_irResults)) {
            if (_irResults.value) {
                __LDBG_printf("IR %08x", _irResults.value);
            }
            _irReceiver->resume();
        }
    });
}

void ClockPlugin::endIRReceiver()
{
    __LDBG_printf("end");
    _Timer(_irTimer).remove();
    _irReceiver->disableIRIn();
    delete _irReceiver;
    _irReceiver = nullptr;
}

#endif
