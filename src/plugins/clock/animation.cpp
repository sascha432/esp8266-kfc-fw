/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "animation.h"
#include "clock.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Clock;


Color Animation::_getColor() const
{
    return _parent._getColor();
}

void Animation::_setColor(Color color)
{
    _parent._setColor(color, false);
    _parent._webUIUpdateColor(color);
}

CoordinateType Animation::getCols() const
{
    return _parent._display.getCols();
}

CoordinateType Animation::getRows() const
{
    return _parent._display.getRows();
}

PixelAddressType Animation::getNumPixels() const
{
    return getCols() * getRows();
}
