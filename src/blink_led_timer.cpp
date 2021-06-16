/**
 * Author: sascha_lammers@gmx.de
 */

#include "blink_led_timer.h"
#include "kfc_fw_config.h"

using KFCConfigurationClasses::System;
using Device = KFCConfigurationClasses::System::Device;

#if 0
#include "debug_helper_enable.h"
#endif

BlinkLEDTimer *ledTimer = nullptr;

#if __LED_BUILTIN == NEOPIXEL_PIN_ID

#include <LoopFunctions.h>
#include <reset_detector.h>
#include "NeoPixel_esp.h"
#include "SpeedBooster.h"

class WS2812LEDTimer : public BlinkLEDTimer {
public:
    WS2812LEDTimer() : BlinkLEDTimer()  {
        digitalWrite(__LED_BUILTIN_WS2812_PIN, LOW);
        pinMode(__LED_BUILTIN_WS2812_PIN, OUTPUT);
    }

    void off() {
        solid(0);
    }

    void solid(uint32_t color) {
        NeoPixel_fillColor(_pixels, sizeof(_pixels), color);
        NeoPixel_espShow(__LED_BUILTIN_WS2812_PIN, _pixels, sizeof(_pixels), true);
    }

    void set(uint32_t delay, uint8_t pin, dynamic_bitset &pattern)
    {
        off();
        _pattern = pattern;
        startTimer(delay, true);
    }

    void setColor(uint32_t color) {
        _color = color;
    }

    virtual void run() override {
        auto state = _pattern.test(_counter++ % _pattern.size());
        if (!state) {
            off();
        }
        else {
            solid(_color);
        }
    }

    virtual void detach() override {
        off();
        OSTimer::detach();
    }

private:
    uint32_t _color;
    uint8_t _pixels[__LED_BUILTIN_WS2812_NUM_LEDS * 3];
};

#endif

void BlinkLEDTimer::run()
{
    _digitalWrite(_pin, BUILTIN_LED_STATE(_pattern.test(_counter++ % _pattern.size())));
}

// void BlinkLEDTimer::set(uint16_t delay, Bitset &&pattern)
// {
//     if (!isPinValid(_pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
//         return;
//     }
//     _pattern = std::move(pattern);
//     // reset pin
//     _digitalWrite(_pin, BUILTIN_LED_STATE(false));
//     _pinMode(_pin, OUTPUT);
//     _counter = 0;
//     _delay = static_cast<BlinkType>(delay);
//     setInverted(false);
//     __LDBG_printf("start timer=%u", delay);
//     startTimer(delay, true);
// }

void BlinkLEDTimer::detach()
{
    _digitalWrite(_pin, BUILTIN_LED_STATE(false));
    OSTimer::detach();
}

void BlinkLEDTimer::setPattern(uint8_t pin, uint16_t delay, Bitset &&pattern)
{
    if (!isPinValid(pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return;
    }
    if (!ledTimer) {
        ledTimer = new BlinkLEDTimer();
    }
    ledTimer->set(delay, pin, std::move(pattern));
}

void BlinkLEDTimer::set(uint16_t delay, uint8_t pin, Bitset &&pattern)
{
    if (pin != _pin && !isPinValid(_pin)) {
        // disable LED on pin that was previously used
        _digitalWrite(_pin, low());
        _pinMode(_pin, INPUT);
    }
    // check if new pin is valid
    if (!isPinValid(pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return;
    }
    _pin = pin;
    _pattern = std::move(pattern);
    // reset pin
    _digitalWrite(_pin, low());
    _pinMode(_pin, OUTPUT);
    _counter = 0;
    _delay = static_cast<BlinkType>(delay);
    __LDBG_printf("start timer=%u", delay);
    startTimer(delay, true);
}

void BlinkLEDTimer::setBlink(uint8_t pin, uint16_t delay, int32_t color)
{
    if (!isPinValid(pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return;
    }

#if __LED_BUILTIN == NEOPIXEL_PIN_ID
    if (pin == NEOPIXEL_PIN) {
        __LDBG_printf("WS2812 pin=%u, num=%u, delay=%u", __LED_BUILTIN_WS2812_PIN, __LED_BUILTIN_WS2812_NUM_LEDS, delay);
    } else
#endif
    {
        __LDBG_printf("PIN %u, blink %d", pin, delay);
    }

    if (ledTimer) {
        delete ledTimer;
        ledTimer = nullptr;
    }

#if __LED_BUILTIN == NEOPIXEL_PIN_ID
    if (pin == NEOPIXEL_PIN) {
        auto timer = new WS2812LEDTimer();
        if (delay == BlinkLEDTimer::OFF) {
            timer->off();
        }
        else if (delay == BlinkLEDTimer::SOLID) {
            timer->solid(color == -1 ? 0x001500 : color);  // green
        }
        else {
            Bitset pattern;
            if (delay == BlinkLEDTimer::SOS) {
                timer->setColor(color == -1 ? 0x300000 : color);  // red
                pattern.set<uint64_t>(0b000000010101000011110000111100001111010101ULL, 42);
                delay = 200;
            } else {
                pattern.set<uint8_t>(0b10, 2);
                delay = std::max<uint16_t>(50, std::min<uint16_t>(delay, 5000));
                timer->setColor(color == -1 ? ((delay < 100) ? 0x050500 : 0x000010) : color);  // yellow / blue
            }
            timer->set(delay, pin, pattern);
        }
        ledTimer = timer;
    }
    else
#endif
    {
        // reset pin
        _digitalWrite(pin, BUILTIN_LED_STATE(false));
        _pinMode(pin, OUTPUT);

        if (delay == static_cast<uint16_t>(BlinkLEDTimer::BlinkType::OFF)) {
            // already off
            // _digitalWrite(pin, BUILTIN_LED_STATE(false));
        }
        else if (delay == static_cast<uint16_t>(BlinkLEDTimer::BlinkType::SOLID)) {
            _digitalWrite(pin, BUILTIN_LED_STATE(true));
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
        }
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
