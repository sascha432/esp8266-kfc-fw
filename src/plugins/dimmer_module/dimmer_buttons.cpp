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


DimmerButtonsImpl::DimmerButtonsImpl()
{
    pinMonitor.begin();
}

void DimmerButtons::_beginButtons()
{
    static_assert(IOT_DIMMER_MODULE_CHANNELS <= 8, "max. channels exceeded");

    SingleClickGroupPtr group;

    for(uint8_t i = 0; i < _channels.size() * 2; i++) {
        auto pinNum = _config.pin(i);
        if (i % 2 == 0) {
            group.reset(_config.single_click_time); // create new group for each channel
        }
        __LDBG_printf("channel=%u btn=%u pin=%u lp_time=%u steps=%u group=%p", i / 2, i % 2, pinNum, _config.longpress_time, _config.shortpress_steps, group.get());
        if (pinNum) {
            pinMonitor.attach<DimmerButton>(pinNum, i / 2, i % 2, *this, group);
            pinMode(pinNum, IOT_DIMMER_MODULE_PINMODE);
        }
    }
}

void DimmerButtons::_endButtons()
{
    pinMonitor.detach(static_cast<const void *>(this));
}

#endif
