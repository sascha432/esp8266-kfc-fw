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

void ClockPlugin::_loop()
{
    LoopOptionsType options(*this);
    _display.setBrightness(_getBrightness());

    if (_animation) {
        _animation->loop(options.getMillis());
    }
    if (_blendAnimation) {
        _blendAnimation->loop(options.getMillis());
    }

    while(true) {

        if (!options.doUpdate()) {
            break;
        }

        // start update process
        _lastUpdateTime = millis();

        #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
            if (_loopDisplayLightSensor(options)) {
                break;

            }
        #endif

        #if IOT_CLOCK_PIXEL_SYNC_ANIMATION
            if (_loopSyncingAnimation(options)) {
                break;
            }
        #endif

        if (options.doRedraw()) {

            if (_blendAnimation) {

                _animation->nextFrame(options.getMillis());
                _blendAnimation->nextFrame(options.getMillis());

                if (_blendAnimation->blend(_display, options.getMillis())) {
                    // display mixed state
                    _display.show();
                }
                else {
                    // blending done, delete _blendAnimation
                    __LDBG_printf("blending done");
                    delete _blendAnimation;
                    _blendAnimation = nullptr;
                    _display.show();
                }

            }
            else if (_animation) {

                // render single frame
                _animation->nextFrame(options.getMillis());
                _animation->copyTo(_display, options.getMillis());
                _display.show();

            }
            else {

                // no animation object available
                // display plain color
                _display.fill(_getColor());
                _display.show();
            }

            return;
        }

        break;
    }

    if (options.doRefresh() || _config.dithering) {
        // refresh display brightness
        _display.show();
        delayMicroseconds(750);
    }

    // __LDBG_printf("refresh fading_brightness=%u color=%s fps=%u", _fadingBrightness, getColor().toString().c_str(), FastLED.getFPS());
}


#endif
