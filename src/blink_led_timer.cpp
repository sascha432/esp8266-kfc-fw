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

void BlinkLEDTimer::set(uint32_t delay, dynamic_bitset &&pattern)
{
    if (!isPinValid(_pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return;
    }
    _pattern = std::move(pattern);
    // reset pin
    _digitalWrite(_pin, BUILTIN_LED_STATE(false));
    _pinMode(_pin, OUTPUT);
    _counter = 0;
    __LDBG_printf("start timer=%u", delay);
    startTimer(delay, true);
}

void BlinkLEDTimer::detach()
{
    _digitalWrite(_pin, BUILTIN_LED_STATE(false));
    OSTimer::detach();
}

// void setPattern(uint8_t pin, int delay, const dynamic_bitset &pattern)
// {
//     ledTimer->set(delay, pin, std::move(dynamic_bitset(pattern)));
// }

void BlinkLEDTimer::setPattern(uint8_t pin, int delay, dynamic_bitset &&pattern)
{
    if (!isPinValid(pin) || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return;
    }
    if (!ledTimer) {
        ledTimer = __LDBG_new(BlinkLEDTimer);
    }
    ledTimer->set(delay, pin, std::move(pattern));
}

// bool BlinkLEDTimer::isOn() {
//     return ledTimer->_on;
// }


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
        __LDBG_delete(ledTimer);
        ledTimer = nullptr;
    }

#if __LED_BUILTIN == NEOPIXEL_PIN_ID
    if (pin == NEOPIXEL_PIN) {
        auto timer = __LDBG_new(WS2812LEDTimer);
        if (delay == BlinkLEDTimer::OFF) {
            timer->off();
        }
        else if (delay == BlinkLEDTimer::SOLID) {
            timer->solid(color == -1 ? 0x001500 : color);  // green
        }
        else {
            dynamic_bitset pattern;
            if (delay == BlinkLEDTimer::SOS) {
                timer->setColor(color == -1 ? 0x300000 : color);  // red
                pattern.setMaxSize(24);
                pattern.setValue(0xcc1c71d5);
                delay = 200;
            } else {
                pattern.setMaxSize(2);
                pattern = 0b10;
                delay = std::max((uint16_t)50, std::min(delay, (uint16_t)5000));
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
            ledTimer = __LDBG_new(BlinkLEDTimer, pin);
            dynamic_bitset pattern;
            if (delay == static_cast<uint16_t>(BlinkLEDTimer::BlinkType::SOS)) {
                pattern.setMaxSize(42);
                pattern.setValue64(0b000000010101000011110000111100001111010101ULL);
                delay = 200;
            } else {
                pattern.setMaxSize(2);
                pattern = 0b10;
                delay = std::max<uint16_t>(50, std::min<uint16_t>(delay, 5000));
            }

            __LDBG_printf("PIN %u, delay %u, pattern %s", pin, delay, pattern.toString().c_str());
            ledTimer->set(delay, std::move(pattern));
        }
    }
}
