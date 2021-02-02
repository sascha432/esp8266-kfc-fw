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

#if IOT_CLOCK_USE_DITHERING == 0
#define _dithering()
#endif

void ClockPlugin::_loop()
{
#if IOT_CLOCK_HAVE_ENABLE_PIN
    // save energy if the LEDs are disabled
    if (!_isEnabled) {
        delay(5);
        return;
    }
#endif

    LoopOptionsType options(*this);

    while(true) {

        #if IOT_CLOCK_BUTTON_PIN
            // check buttons
            if (_loopUpdateButtons(options)) {
                if (options.doRefresh()) {
                    break;
                }
                _dithering();
                return;
            }
        #endif

        if (!options.doUpdate()) {
            if (options.doRefresh()) {
                break;
            }
            _dithering();
            return;
        }

        // start update process
        #if IOT_CLOCK_DEBUG_ANIMATION_TIME
            MicrosTimer mt;
            _timerDiff.push_back(options.getTimeSinceLastUpdate())
        #endif
        _lastUpdateTime = millis();

        #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
            if (_loopDisplayLightSensor(options)) {
                _dithering();
                return;
            }
        #endif

        #if IOT_CLOCK_PIXEL_SYNC_ANIMATION
            if (_loopSyncingAnimation(options)) {
                _dithering();
                return;
            }
        #endif

        if (_animation) {
            #if IOT_CLOCK_DEBUG_ANIMATION_TIME
                mt.start()
                _animation->loop(options.getMillis());
                _anmationTime.push_back(mt.getTime());
            #else
                _animation->loop(options.getMillis());
            #endif
            if (_animation->finished()) {
                __LDBG_printf("animation loop has finished");
                setAnimation(AnimationType::NEXT);
            }
        }

        if (options.doRedraw()) {
            #if IOT_CLOCK_DEBUG_ANIMATION_TIME
                mt.start()
                _display.setParams(options.getMillis(), options.getBrightness());
                _display.setPixels(_getColor());
                _animationRenderTime.push_back(mt.getTime());

                mt.start();
                _display.show();
                _displayTime.push_back(mt.getTime());
            #else
                _display.setParams(options.getMillis(), options.getBrightness());
                _display.setPixels(_getColor());
                _display.show();
            #endif
        }
        else {
            if (options.doRefresh()) {
                break;
            }
            _dithering();
        }

        return;
    }

    _display.setBrightness(_fadingBrightness = options.getBrightness());
    _display.setPixels(_getColor());
    _display.show();

    // __LDBG_printf("refresh fading_brightness=%u color=%s fps=%u", _fadingBrightness, getColor().toString().c_str(), FastLED.getFPS());
}


#endif
