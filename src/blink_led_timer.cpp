/**
 * Author: sascha_lammers@gmx.de
 */

#include "blink_led_timer.h"
#include "kfc_fw_config.h"

BlinkLEDTimer ledTimer;

BlinkLEDTimer::BlinkLEDTimer() : OSTimer() {
    _pin = INVALID_PIN;
}

void BlinkLEDTimer::run() {
    digitalWrite(_pin, BUILTIN_LED_STATE(_pattern.test(_counter++ % _pattern.size())));
}

void BlinkLEDTimer::set(uint32_t delay, int8_t pin, dynamic_bitset &pattern) {
    _pattern = pattern;
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

void BlinkLEDTimer::detach() {
    if (_pin != INVALID_PIN) {
        analogWrite(_pin, 0);
        digitalWrite(_pin, BUILTIN_LED_STATE(false));
    }
    OSTimer::detach();
}

void BlinkLEDTimer::setPattern(int8_t pin, int delay, dynamic_bitset &pattern) {

    // _debug_printf_P(PSTR("blink_led pin %d, pattern %s, delay %d\n"), pin, pattern.toString().c_str(), delay);
    ledTimer.set(delay, pin, pattern);
}

void BlinkLEDTimer::setBlink(int8_t pin, uint16_t delay) {

    auto flags = config._H_GET(Config().flags);

    if (pin == DEFAULT_PIN) {
        auto ledPin = config._H_GET(Config().led_pin);
        pin = (flags.ledMode != MODE_NO_LED) ? ledPin : INVALID_PIN;
        // _debug_printf_P(PSTR("Using configured LED mode %d, PIN %d=%d, blink %d\n"), flags.ledMode, ledPin, pin, delay);
    } else {
        // _debug_printf_P(PSTR("PIN %d, blink %d\n"), pin, delay);
    }

    ledTimer.detach();

    if (pin >= 0) {
        // reset pin
        analogWrite(pin, 0);
        digitalWrite(pin, BUILTIN_LED_STATE(false));

        if (delay == BlinkLEDTimer::OFF) {
            digitalWrite(pin, BUILTIN_LED_STATE(false));
        } else if (delay == BlinkLEDTimer::SOLID) {
            digitalWrite(pin, BUILTIN_LED_STATE(true));
        } else {
            dynamic_bitset pattern;
            if (delay == BlinkLEDTimer::SOS) {
                pattern.setMaxSize(24);
                pattern.setValue(0xcc1c71d5);
                delay = 200;
            } else {
                pattern.setMaxSize(2);
                pattern = 0b10;
                delay = _max(50, _min(delay, 5000));
            }
            ledTimer.set(delay, pin, pattern);
        }
    }
}
