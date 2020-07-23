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

Clock::Color::Color()
{
}

Clock::Color::Color(uint8_t *values) : _red(values[0]), _green(values[1]), _blue(values[2])
{
}

Clock::Color::Color(uint8_t red, uint8_t green, uint8_t blue) : _red(red), _green(green), _blue(blue)
{
}

Clock::Color::Color(uint32_t value, bool bgr)
{
    if (bgr) {
        _blue = value >> 16;
        _green = value >> 8;
        _red = value;
    } else {
        *this = value;
    }
}

Clock::Color Clock::Color::fromString(const String &value)
{
    auto ptr = value.c_str();
    while(isspace(*ptr) || *ptr == '#') {
        ptr++;
    }
    return Color(static_cast<uint32_t>(strtol(ptr, nullptr, 16) & 0xffffff));
}

String Clock::Color::toString() const
{
    char buf[8];
    snprintf_P(buf, sizeof(buf), PSTR("#%02X%02X%02X"), _red, _green, _blue);
    return buf;
}

uint32_t Clock::Color::rnd()
{
    do {
        *this = Color((rand() % 4) * (255 / 4), (rand() % 4) * (255 / 4), (rand() % 4) * (255 / 4));
    } while(_red == 0 && _green == 0 && _blue == 0);
    return get();
}

uint32_t Clock::Color::get()
{
    return ((uint32_t)_red << 16) | ((uint32_t)_green <<  8) | _blue;
}

Clock::Color &Clock::Color::operator =(uint32_t value)
{
    _red = value >> 16;
    _green = value >> 8;
    _blue = value;
    return *this;
}

Clock::Color::operator int()
{
    return get();
}

uint32_t Clock::Color::get(uint8_t red, uint8_t green, uint8_t blue)
{
    return ((uint32_t)red << 16) | ((uint32_t)green <<  8) | blue;
}

uint8_t Clock::Color::red() const
{
    return _red;
}

uint8_t Clock::Color::green() const
{
    return _green;
}

uint8_t Clock::Color::blue() const
{
    return _blue;
}


void Clock::Animation::loop(time_t now)
{
    if (_callback) {
        _callback(now);
    }
}

Clock::FadingAnimation::FadingAnimation(ClockPlugin &clock, Color from, Color to, float speed, uint16_t time) : Animation(clock), _from(from), _to(to), _time(time), _speed(_secondsToSpeed(speed))
{
    __LDBG_printf("from=%s to=%s time=%u speed=%u (%f)", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, speed);
}

void Clock::FadingAnimation::begin()
{
    _finished = false;
    _progress = 0;
    _clock.setUpdateRate(kUpdateRate);
    __LDBG_printf("begin from=%s to=%s time=%u speed=%u update_rate=%u", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, _clock._updateRate);

    _callback = [this](time_t now) {
        this->callback(now);
    };
}

uint16_t Clock::FadingAnimation::_secondsToSpeed(float seconds) const
{
    return (kProgressPerSecond / seconds) + 0.5;
}

void Clock::FadingAnimation::callback(time_t now)
{
    if (_progress < kMaxProgress) {
        _progress = std::min((uint32_t)kMaxProgress, (uint32_t)(_progress + _speed));
        float stepRed = (_to.red() - _from.red()) / kFloatMaxProgress;
        float stepGreen = (_to.green() - _from.green()) / kFloatMaxProgress;
        float stepBlue = (_to.blue() - _from.blue()) / kFloatMaxProgress;
        _clock.setColor(Color(
            (uint8_t)(_from.red() + (stepRed * (float)_progress)),
            (uint8_t)(_from.green() + (stepGreen * (float)_progress)),
            (uint8_t)(_from.blue() + (stepBlue * (float)_progress))
        ));
        if (_progress >= kMaxProgress) {
            if (_time == kNoColorChange) {
                _finished = true;
                _callback = nullptr;
                __LDBG_printf("end no color change");
            }
            else if (_time == 0) {
                _from = _to;
                _to.rnd();
                _progress = 0;
                __LDBG_printf("next from=%s to=%s time=%u speed=%u update_rate=%u", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, _clock._updateRate);
            } else {
                // wait for _time seconds
                uint32_t endTime = now + _time;
                __LDBG_printf("delay=%u time=%u", _time, endTime);
                _callback = [this, endTime](time_t now) {
                    if ((uint32_t)now > endTime) {
                        // get current color and a random new color
                        _from = _clock.getColor();
                        _to.rnd();
                        begin();
                    }
                };
            }
        }
    }
}


Clock::RainbowAnimation::RainbowAnimation(ClockPlugin &clock, uint16_t speed, float multiplier, Color factor) :
    Animation(clock),
    _speed(speed),
    _factor(factor),
    _multiplier(multiplier * SevenSegmentDisplay::getTotalPixels())
{
    __LDBG_printf("speed=%u _multiplier=%f multiplier=%f factors=%u,%u,%u", _speed, _multiplier, multiplier, factor.red(), factor.green(), factor.blue());
}

void Clock::RainbowAnimation::begin()
{
    _finished = false;
    _clock._display.setCallback([this](PixelAddressType address, ColorType color) {

        uint32_t ind = (address * _multiplier) + (millis() / _speed);
        uint8_t indMod = (ind % _mod);
        uint8_t idx = (indMod / _divMul);
        float factor1 = 1.0 - ((float)(indMod - (idx * _divMul)) / _divMul);
        float factor2 = (float)((int)(ind - (idx * _divMul)) % _mod) / _divMul;

        switch(idx) {
            case 0:
                return Color::get(_factor.red() * factor1, _factor.green() * factor2, 0);
            case 1:
                return Color::get(0, _factor.green() * factor1, _factor.blue() * factor2);
            case 2:
                return Color::get(_factor.red() * factor2, 0, _factor.blue() * factor1);
        }
        return color;
    });
    _clock.setUpdateRate(1000 / _speed);
    __LDBG_printf("begin rate=%u", _clock._updateRate);
}

void Clock::RainbowAnimation::end()
{
    _clock._display.setCallback(nullptr);
    _clock.setUpdateRate(100);
    _finished = true;
    __LDBG_printf("end rate=%u", _clock._updateRate);
}

Clock::FlashingAnimation::FlashingAnimation(ClockPlugin &clock, Color color, uint16_t time) : Animation(clock), _color(color), _time(time)
{
    __LDBG_printf("color=%s time=%u", _color.toString().c_str(), _time);
}

void Clock::FlashingAnimation::begin()
{
    __LDBG_printf("begin color=%s time=%u", _color.toString().c_str(), _time);
    _clock._display.setCallback([this](PixelAddressType addr, ColorType color) {
        return (millis() / _clock._updateRate) % 2 == 0 ? (uint32_t)_color : 0U;
    });
    _clock.setUpdateRate(std::max(ClockPlugin::kMinFlashingSpeed, _time));
}

void Clock::FlashingAnimation::end()
{
    __LDBG_printf("end color=%s time=%u", _color.toString().c_str(), _time);
    _clock._display.setCallback(nullptr);
    _clock.setUpdateRate(100);
}
