/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#include "../include/templates.h"
#include <plugins.h>
#include "dimmer_button.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Dimmer;
using namespace PinMonitor;

static_assert(IOT_DIMMER_MODULE_CHANNELS <= 8, "max. channels exceeded");

// pinMonitor.begin() is only required if the timer is used
// ButtonsImpl::ButtonsImpl() : Base()
// {
//     pinMonitor.begin(true);
// }

void Buttons::begin()
{
    SingleClickGroupPtr group;

    pinMonitor.setDefaultPinMode(IOT_DIMMER_MODULE_PINMODE);

    for(uint8_t i = 0; i < _channels.size() * 2; i++) {
        auto pinNum = _config._base.pin(i);
        __LDBG_assert_printf(pinNum != 0xff, "pinNum=0x%02x channel=%u", pinNum, i);
        if (pinNum != 0xff) {
            if (i % 2 == 0) {
                group.reset(_config._base.single_click_time); // create new group for each channel
            }
            __LDBG_printf("channel=%u btn=%u pin=%u lp_time=%u steps=%u group=%p", i / 2, i % 2, pinNum, _config._base.longpress_time, _config._base.shortpress_steps, group.get());
            if (pinNum) {
                pinMonitor.attach<Button>(pinNum, i / 2, i % 2, *this, group);
            }
        }
    }

    pinMonitor.begin();
}

#endif
