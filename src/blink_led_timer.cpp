/**
 * Author: sascha_lammers@gmx.de
 */

#include "blink_led_timer.h"
#include "kfc_fw_config.h"
#include <stl_ext/memory.h>

#if DEBUG_BLINK_LED_TIMER
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

#if BUILTIN_LED_NEOPIXEL
#   if HAVE_FASTLED
        CRGB WS2812LEDTimer::_pixels[__LED_BUILTIN_WS2812_NUM_LEDS];
#   else
        NeoPixelEx::Strip<__LED_BUILTIN_WS2812_PIN, __LED_BUILTIN_WS2812_NUM_LEDS, NeoPixelEx::GRB, NeoPixelEx::TimingsWS2812> WS2812LEDTimer::_pixels;
#   endif
#endif

#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID

using KFCConfigurationClasses::System;
using Device = KFCConfigurationClasses::System::Device;

BlinkLEDTimer *ledTimer = nullptr;

BlinkLEDTimer::BlinkLEDTimer(uint8_t pin) :
    OSTimer(OSTIMER_NAME("BlinkLEDTimer")),
    _pin(pin),
    _delay(BlinkType::INVALID)
{
    if (!isPinValid(_pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        __LDBG_printf("pin=%u valid=%u led_global_off=%u", _pin, isPinValid(_pin), System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF);
        return;
    }
    #if !BUILTIN_LED_NEOPIXEL
        // reset pin
        digitalWrite(_pin, low());
        pinMode(_pin, OUTPUT);
    #endif
}

void BlinkLEDTimer::setPattern(uint8_t pin, uint16_t delay, Bitset &&pattern)
{
    if (!isPinValid(pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        __LDBG_printf("pin=%u valid=%u led_global_off=%u", pin, isPinValid(pin), System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF);
        return;
    }
    stdex::reset(ledTimer, new BlinkLEDTimer());
    ledTimer->set(delay, pin, std::move(pattern));
}

void BlinkLEDTimer::set(uint16_t delay, uint8_t pin, Bitset &&pattern)
{
    // check if new pin is valid
    if (!isPinValid(pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        __LDBG_printf("pin=%u valid=%u led_global_off=%u", pin, isPinValid(pin), System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF);
        return;
    }
    if (pin != _pin) {
        _pin = pin;
        // reset pin
        digitalWrite(_pin, low());
        pinMode(_pin, OUTPUT);
    }
    _pattern = std::move(pattern);
    _counter = 0;
    _delay = static_cast<BlinkType>(delay);
    __LDBG_printf("start timer=%u", delay);
    startTimer(delay, true);
}

void BlinkLEDTimer::setBlink(uint8_t pin, uint16_t delay, int32_t color)
{
    if (!isPinValid(pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        __LDBG_printf("pin=%u valid=%u led_global_off=%u", pin, isPinValid(pin), System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF);
        return;
    }

    #if BUILTIN_LED_NEOPIXEL
        if (pin == NEOPIXEL_PIN) {
            __LDBG_printf("WS2812 pin=%u, num=%u, delay=%u color=%06x", __LED_BUILTIN_WS2812_PIN, __LED_BUILTIN_WS2812_NUM_LEDS, delay, color);
        }
        else
    #endif
    {
        __LDBG_printf("PIN %u, blink %d", pin, delay);
    }

    uint8_t _oldPin = 0xff;
    if (ledTimer) {
        __LDBG_printf("removing timer pin=%u", ledTimer->_pin);
        _oldPin = ledTimer->_pin;
        stdex::reset(ledTimer);
    }

    #if BUILTIN_LED_NEOPIXEL
        if (pin == NEOPIXEL_PIN) {
            __LDBG_printf("creating new WS2812 timer");
            auto timer = new WS2812LEDTimer();
            if (static_cast<BlinkLEDTimer::BlinkType>(delay) == BlinkLEDTimer::BlinkType::OFF) {
                timer->off();
            }
            else if (static_cast<BlinkLEDTimer::BlinkType>(delay) == BlinkLEDTimer::BlinkType::SOLID) {
                timer->solid(color == -1 ? 0x001500 : color);  // green
            }
            else if (static_cast<BlinkLEDTimer::BlinkType>(delay) == BlinkLEDTimer::BlinkType::FLICKER) {
                timer->solid(color == -1 ? 0x050500 : color);  // yellow
            }
            else if (static_cast<BlinkLEDTimer::BlinkType>(delay) == BlinkLEDTimer::BlinkType::SLOW) {
                timer->solid(color == -1 ? 0x500000 : color);  // red
            }
            else if (static_cast<BlinkLEDTimer::BlinkType>(delay) == BlinkLEDTimer::BlinkType::MEDIUM) {
                timer->solid(color == -1 ? 0x000050 : color);  // blue
            }
            else {
                Bitset pattern;
                if (static_cast<BlinkLEDTimer::BlinkType>(delay) == BlinkLEDTimer::BlinkType::SOS) {
                    timer->setColor(color == -1 ? 0x300000 : color);  // red
                    pattern.set<uint64_t>(0b000000010101000011110000111100001111010101ULL, 42);
                    delay = 200;
                }
                else {
                    pattern.set<uint8_t>(0b10, 2);
                    delay = std::clamp<uint16_t>(delay, 50, 5000);
                    timer->setColor(color == -1 ? ((delay < 100) ? 0x050500 : 0x000010) : color);  // yellow / blue
                }
                timer->set(delay, pin, pattern);
            }
            stdex::reset(ledTimer, timer);
            __LDBG_printf("pin=%u NeoPixel active color=%06x", pin, timer->getColor());
            return;
        }
        __LDBG_printf("pin=%u invalid NeoPixel pin", pin);
    #endif

    if (static_cast<BlinkLEDTimer::BlinkType>(delay) == BlinkLEDTimer::BlinkType::OFF) {
        // reset pin
        digitalWrite(pin, BUILTIN_LED_STATE(false));
    }
    else if (static_cast<BlinkLEDTimer::BlinkType>(delay) == BlinkLEDTimer::BlinkType::SOLID) {
        digitalWrite(pin, BUILTIN_LED_STATE(true));
    }
    else {
        ledTimer = new BlinkLEDTimer(pin);
        Bitset pattern;
        if (delay == static_cast<uint16_t>(BlinkLEDTimer::BlinkType::SOS)) {
            pattern.set<uint64_t>(0b000000010101000011110000111100001111010101ULL, 42);
            delay = 200;
        }
        else {
            pattern.set<uint8_t>(0b10, 2);
            delay = std::max<uint16_t>(50, std::min<uint16_t>(delay, 5000));
        }

        __LDBG_printf("PIN %u, delay %u, pattern %s", pin, delay, pattern.toString().c_str());
        ledTimer->set(delay, std::move(pattern));
        return;
    }
    if (_oldPin != pin) {
        pinMode(pin, OUTPUT);
    }
}

bool BlinkLEDTimer::isBlink(uint8_t pin, BlinkType delay)
{
    if (!isPinValid(pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return true;
    }
    if (!ledTimer) {
        return false;
    }
    if (ledTimer->_pin != pin) {
        return false;
    }
    if (ledTimer->_pattern.size() != 2 || static_cast<uint8_t>(ledTimer->_pattern) != 0b10) {
        return false;
    }
    return ledTimer->_delay == delay;
}

#endif
