/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <ReadADC.h>
#include "clock.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace LEDMatrix;

LoopOptionsType::LoopOptionsType(ClockPlugin &plugin) :
    _plugin(plugin),
    _millis(millis()),
    _millisSinceLastUpdate(get_time_diff(_plugin._lastUpdateTime, _millis))
{
}

uint32_t LoopOptionsType::getTimeSinceLastUpdate() const
{
    return _millisSinceLastUpdate;
}

uint32_t LoopOptionsType::getMillis() const
{
    return _millis;
}

bool LoopOptionsType::doUpdate() const
{
    return (_millisSinceLastUpdate >= (_plugin._isFading ? (1000 / 50/*Hz*/) : _plugin._updateRate));
}

bool LoopOptionsType::doRedraw()
{
    if (_plugin._forceUpdate || doUpdate()) {
        _plugin._forceUpdate = false;
        return true;
    }
    return false;
}




bool ClockPlugin::_loopSyncingAnimation(LoopOptionsType &options)
{
    return false;
}

bool ClockPlugin::_loopDisplayLightSensor(LoopOptionsType &options)
{
    return false;
}

bool ClockPlugin::_loopUpdateButtons(LoopOptionsType &options)
{
    return false;
}

void ClockPlugin::_loop()
{
    LoopOptionsType options(*this);

    // check buttons
    if (_loopUpdateButtons(options)) {
        return;
    }
    if (!options.doUpdate()) {
        return;
    }

    // start update process
    __DBGTM(
        MicrosTimer mt;
        _timerDiff.push_back(options.getTimeSinceLastUpdate())
    );
    _lastUpdateTime = millis();

    if (_loopDisplayLightSensor(options)) {
        return;
    }
    else if (_loopSyncingAnimation(options)) {
        return;
    }

    if (_animation) {
        __DBGTM(mt.start());
        _animation->loop(options.getNow());
        __DBGTM(_anmationTime.push_back(mt.getTime()));
        if (_animation->finished()) {
            __LDBG_printf("animation loop has finished");
            setAnimation(AnimationType::NEXT);
        }
    }

    if (options.doRedraw()) {

        uint32_t color = _color;

        if (_isFading && _fadeTimer.reached()) {
            __LDBG_printf("fading=done brightness=%u", _targetBrightness);
            _isFading = false;
            _schedulePublishState = true;
        }

        __DBGTM(mt.start());
        _display.setParams(options.getMillis(), _getBrightness());
        _display.setPixels(color);
        __DBGTM(_animationRenderTime.push_back(mt.getTime()));

        __DBGTM(mt.start());
        _display.show();
        __DBGTM(_displayTime.push_back(mt.getTime()));
    }
}


#endif
