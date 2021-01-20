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

#if __LED_BUILTIN == -3

#include <LoopFunctions.h>
#include <reset_detector.h>
#include "NeoPixel_esp.h"
#include "SpeedBooster.h"

class WS2812LEDTimer : public BlinkLEDTimer {
public:
    WS2812LEDTimer() : BlinkLEDTimer()  {
        pinMode(__LED_BUILTIN_WS2812_PIN, OUTPUT);
        digitalWrite(__LED_BUILTIN_WS2812_PIN, LOW);
    }

    void off() {
        solid(0);
    }

    void solid(uint32_t color) {
        NeoPixel_fillColor(_pixels, sizeof(_pixels), color);
        NeoPixel_espShow(__LED_BUILTIN_WS2812_PIN, _pixels, sizeof(_pixels), true);
    }

    void set(uint32_t delay, int8_t pin, dynamic_bitset &pattern)
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

BlinkLEDTimer::BlinkLEDTimer() : _pin(INVALID_PIN)//, _on(false)
{
}

void BlinkLEDTimer::run()
{
    digitalWrite(_pin, BUILTIN_LED_STATE(_pattern.test(_counter++ % _pattern.size())));
}

void BlinkLEDTimer::set(uint32_t delay, int8_t pin, dynamic_bitset &&pattern)
{
    if (_pin == INVALID_PIN || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return;
    }
    _pattern = std::move(pattern);
    //_on = pattern.size();
    if (_pin != pin) {
        // set PIN as output
        _pin = pin;
        pinMode(_pin, OUTPUT);
        analogWrite(pin, 0);
        digitalWrite(pin, BUILTIN_LED_STATE(false));
    }
    if (_pin != INVALID_PIN) {
        _counter = 0;
        startTimer(delay, true);
    } else {
        detach();
    }
}

void BlinkLEDTimer::detach()
{
    if (_pin <= INVALID_PIN) {
        analogWrite(_pin, 0);
        digitalWrite(_pin, BUILTIN_LED_STATE(false));
    }
    OSTimer::detach();
}

// void setPattern(int8_t pin, int delay, const dynamic_bitset &pattern)
// {
//     ledTimer->set(delay, pin, std::move(dynamic_bitset(pattern)));
// }

void BlinkLEDTimer::setPattern(int8_t pin, int delay, dynamic_bitset &&pattern)
{
    if (pin == INVALID_PIN || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return;
    }
    ledTimer->set(delay, pin, std::move(pattern));
}

// bool BlinkLEDTimer::isOn() {
//     return ledTimer->_on;
// }


void BlinkLEDTimer::setBlink(int8_t pin, uint16_t delay, int32_t color)
{
    if (pin == INVALID_PIN || System::Device::getConfig().getStatusLedMode() == System::Device::StatusLEDModeType::OFF) {
        return;
    }

#if __LED_BUILTIN == -3
    if (pin == -3) {
        __LDBG_printf("WS2812 pin=%u, num=%u, delay=%u", __LED_BUILTIN_WS2812_PIN, __LED_BUILTIN_WS2812_NUM_LEDS, delay);
    } else
#endif
    {
        __LDBG_printf("PIN %d, blink %d", pin, delay);
    }

    if (ledTimer) {
        __LDBG_delete(ledTimer);
        ledTimer = nullptr;
    }

#if __LED_BUILTIN == -3
    if (pin == -3) {
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
        ledTimer = __LDBG_new(BlinkLEDTimer);
        if (pin >= 0) {
            // reset pin
            analogWrite(pin, 0);
            digitalWrite(pin, BUILTIN_LED_STATE(false));
//            ledTimer->_on = false;

            if (delay == static_cast<uint16_t>(BlinkLEDTimer::BlinkType::OFF)) {
                digitalWrite(pin, BUILTIN_LED_STATE(false));
            }
            else if (delay == static_cast<uint16_t>(BlinkLEDTimer::BlinkType::SOLID)) {
                digitalWrite(pin, BUILTIN_LED_STATE(true));
                // ledTimer->_on = true;
            }
            else {
                dynamic_bitset pattern;
                if (delay == static_cast<uint16_t>(BlinkLEDTimer::BlinkType::SOS)) {
                    pattern.setMaxSize(24);
                    pattern.setValue(0xcc1c71d5);
                    delay = 200;
                } else {
                    pattern.setMaxSize(2);
                    pattern = 0b10;
                    delay = std::max<uint16_t>(50, std::min<uint16_t>(delay, 5000));
                }
                ledTimer->set(delay, pin, std::move(pattern));
            }
        }
    }
}
