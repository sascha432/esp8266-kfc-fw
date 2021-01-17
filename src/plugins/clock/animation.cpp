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

// ------------------------------------------------------------------------
// Color Class

Color::Color() : _value(0)
{
}

Color::Color(uint8_t *values) : _blue(values[0]), _green(values[1]), _red(values[2])
{
}

Color::Color(uint8_t red, uint8_t green, uint8_t blue) : _blue(blue), _green(green), _red(red)
{
}

Color::Color(uint32_t value) : _value(value)
{
}

Color Color::fromString(const String &value)
{
    auto ptr = value.c_str();
    while(isspace(*ptr) || *ptr == '#') {
        ptr++;
    }
    return static_cast<uint32_t>(strtol(ptr, nullptr, 16) & 0xffffff);
}

Color Color::fromBGR(uint32_t value)
{
    return Color(value, value >> 8, value >> 16);
}

String Color::toString() const
{
    char buf[8];
    //snprintf_P(buf, sizeof(buf), PSTR("#%02X%02X%02X"), _red, _green, _blue);
    snprintf_P(buf, sizeof(buf), PSTR("#%06X"), static_cast<int>(_value));
    return buf;
}

String Color::implode(char sep) const
{
    char buf[16];
    snprintf_P(buf, sizeof(buf), PSTR("%u%c%u%c%u"), _red, sep, _green, sep, _blue);
    return buf;
}

uint8_t Color::_getRand(uint8_t mod, uint8_t mul, uint8_t factor)
{
    return rand() % mod * mul;
    // uint8_t rnd = rand() % mod;
    // if (factor == 255) {
    //     return rnd * mul;
    // }
    // return (rnd * mul * factor) / 255;
}

uint32_t Color::rnd(uint8_t minValue)
{
    do {
        _red = _getRand(kRndMod, kRndMul);
        _green = _getRand(kRndMod, kRndMul);
        _blue = _getRand(kRndMod, kRndMul);
    } while (!_value && !(_red >= minValue || _green >= minValue || _blue >= minValue));

    return _value;
}

// uint32_t Color::rnd(Color factor, uint8_t minValue)
// {
//     uint8_t minRed = red() * minValue / 255;
//     uint8_t count = 0;
//     do {
//         _red = _getRand(kRndMod, kRndMul, red());
//         if (++count == 0) {
//             _red = minValue;
//         }
//     } while (_red < minRed);
//     count = 0;
//     uint8_t minGreen = green() * minValue / 255;
//     do {
//         _green = _getRand(kRndMod, kRndMul, green());
//         if (++count == 0) {
//             _green = minValue;
//         }
//     } while (_green < minGreen);
//     count = 0;
//     uint8_t minBlue = blue() * minValue / 255;
//     do {
//         _blue = _getRand(kRndMod, kRndMul, blue());
//         if (++count == 0) {
//             _blue = minValue;
//         }
//     } while (_blue < minBlue);

//     return _value;
// }

uint32_t Color::get() const
{
    return _value;
}

Color &Color::operator=(uint32_t value)
{
    _value = value;
    return *this;
}

Color::operator bool() const
{
    return _value != 0;
}

Color::operator int() const
{
    return _value;
}

Color::operator uint32_t() const
{
    return _value;
}

bool Color::operator==(uint32_t value) const
{
    return _value == value;
}

bool Color::operator!=(uint32_t value) const
{
    return _value != value;
}

bool Color::operator==(int value) const
{
    return _value == value;
}

bool Color::operator!=(int value) const
{
    return _value != value;
}

uint8_t Color::red() const
{
    return _red;
}

uint8_t Color::green() const
{
    return _green;
}

uint8_t Color::blue() const
{
    return _blue;
}

uint8_t &Color::red()
{
    return _red;
}

uint8_t &Color::green()
{
    return _green;
}

uint8_t &Color::blue()
{
    return _blue;
}

// ------------------------------------------------------------------------
// Base Class Animation

Animation::Animation(ClockPlugin &clock) : _clock(clock), _updateRate(_clock.getUpdateRate()), _finished(false), _blinkColon(true)
{
}

Animation::~Animation()
{
    if (!_finished) {
        end();
    }
    removeAnimationCallback();
}


void Animation::begin()
{
    __LDBG_printf("begin");
    _finished = true;
}
void Animation::end()
{
    __LDBG_printf("end");
    _finished = true;
    _blinkColon = true;
    setUpdateRate(kDefaultUpdateRate);
    removeAnimationCallback();
}

bool Animation::finished() const
{
    return _finished;
}

bool Animation::doBlinkColon() const
{
    return _blinkColon;
}

uint16_t Animation::getUpdateRate() const
{
    return _updateRate;
}

void Animation::loop(time_t now)
{
    if (_callback) {
        _callback(now);
    }
}

void Animation::setUpdateRate(uint16_t updateRate)
{
    _updateRate = updateRate;
    _clock.setUpdateRate(_updateRate);
}

uint16_t Animation::getClockUpdateRate() const
{
    return _clock.getUpdateRate();
}


void Animation::setColor(Color color)
{
    _clock.setColor(color);
}

Color Animation::getColor() const
{
    return _clock.getColor();
}


void Animation::setAnimationCallback(AnimationCallback callback)
{
    _clock.setAnimationCallback(callback);
}

void Animation::removeAnimationCallback()
{
    _clock.setAnimationCallback(nullptr);
}

void Animation::setLoopCallback(LoopCallback callback)
{
    _callback = callback;
}

void Animation::removeLoopCallback()
{
    _callback = nullptr;
}


// ------------------------------------------------------------------------
// Fading Animation

FadingAnimation::FadingAnimation(ClockPlugin &clock, Color from, Color to, float speed, uint16_t time, Color factor) : Animation(clock), _from(from), _to(to), _time(time), _speed(_secondsToSpeed(speed)), _factor(factor)
{
    __LDBG_printf("from=%s to=%s time=%u speed=%u (%f) factor=%s", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, speed, _factor.toString().c_str());
}

void FadingAnimation::begin()
{
    _finished = false;
    _progress = 0;
    setUpdateRate(kUpdateRate);
    __LDBG_printf("begin from=%s to=%s time=%u speed=%u update_rate=%u", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, getUpdateRate());

    setLoopCallback([this](time_t now) {
        this->callback(now);
        // no code here, the lambda function might be destroyed already
    });
}

uint16_t FadingAnimation::_secondsToSpeed(float seconds) const
{
    return (kProgressPerSecond / seconds) + 0.5;
}

void FadingAnimation::callback(time_t now)
{
    if (_progress < kMaxProgress) {
        _progress = std::min((uint32_t)kMaxProgress, (uint32_t)(_progress + _speed));
        float stepRed = (_to.red() - _from.red()) / kFloatMaxProgress;
        float stepGreen = (_to.green() - _from.green()) / kFloatMaxProgress;
        float stepBlue = (_to.blue() - _from.blue()) / kFloatMaxProgress;
        setColor(Color(
            _from.red() + static_cast<uint8_t>(stepRed * _progress),
            _from.green() + static_cast<uint8_t>(stepGreen * _progress),
            _from.blue() + static_cast<uint8_t>(stepBlue * _progress)
        ));
        if (_progress >= kMaxProgress) {
            setColor(_to);
            if (_time == kNoColorChange) {
                _finished = true;
                __LDBG_printf("end no color change");
                removeLoopCallback();
                return; // make sure to return after destroying the lambda function
            }
            else if (_time == 0) {
                _from = _to;
                _to.rnd();
                _progress = 0;
                __LDBG_printf("next from=%s to=%s time=%u speed=%u update_rate=%u", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, getUpdateRate());
            } else {
                // wait for _time seconds
                uint32_t endTime = now + _time;
                __LDBG_printf("delay=%u time=%u", _time, endTime);
                _callback = [this, endTime](time_t now) {
                    if ((uint32_t)now > endTime) {
                        // get current color and a random new color
                        _from = getColor();
                        _to.rnd();
                        begin();
                    }
                };
            }
        }
    }
}

// ------------------------------------------------------------------------
// Rainbow Animation

RainbowAnimation::RainbowAnimation(ClockPlugin &clock, uint16_t speed, RainbowMultiplier &multiplier, RainbowColor &color) :
    Animation(clock),
    _speed(speed),
    _multiplier(multiplier),
    _color(color),
    _factor(color.factor.value)
{
    // __LDBG_printf("speed=%u _multiplier=%f multiplier=%f factor=%s mins=%s", _speed, _multiplier, multiplier, _factor.toString().c_str(), _minimum.toString().c_str());
}

Color RainbowAnimation::_normalizeColor(uint8_t red, uint8_t green, uint8_t blue) const
{
    return Color(
        std::max(red, _color.min.red),
        std::max(green, _color.min.green),
        std::max(blue, _color.min.blue)
    );
}

void RainbowAnimation::_increment(float *valuePtr, float min, float max, float *incrPtr)
{
    float value;
    float incr;
    // we need to use memcpy in case the pointer is not dword aligned
    memcpy(&value, valuePtr, sizeof(value));
    memcpy(&incr, incrPtr, sizeof(incr));
    if (incr) {
        value += incr;
        if (value < min && incr < 0) {
            value = min;
            incr = -incr;
        }
        else if (value >= max && incr > 0) {
            incr = -incr;
            value = max + incr;
        }
    }
    memcpy(valuePtr, &value, sizeof(*valuePtr));
    memcpy(incrPtr, &incr, sizeof(*incrPtr));
}

void RainbowAnimation::begin()
{
    _finished = false;
    setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {

        uint32_t ind = (address * _multiplier.value) + (params.millis / _speed);
        uint8_t indMod = (ind % _mod);
        uint8_t idx = (indMod / _divMul);
        float factor1 = 1.0f - ((float)(indMod - (idx * _divMul)) / _divMul);
        float factor2 = (float)((int)(ind - (idx * _divMul)) % _mod) / _divMul;

        switch(idx) {
            case 0:
                color = _normalizeColor(_factor.red() * factor1, _factor.green() * factor2, 0);
                break;
            case 1:
                color = _normalizeColor(0, _factor.green() * factor1, _factor.blue() * factor2);
                break;
            case 2:
                color = _normalizeColor(_factor.red() * factor2, 0, _factor.blue() * factor1);
                break;
        }
        return SevenSegmentDisplay::_adjustBrightnessAlways(color, params.brightness);
    });
    setUpdateRate(25);
    __LDBG_printf("begin rate=%u", getUpdateRate());
}

void RainbowAnimation::end()
{
    __LDBG_printf("end rate=%u", getUpdateRate());
    Animation::end();
}

void RainbowAnimation::loop(time_t now)
{
    _increment(&_multiplier.value, _multiplier.min, _multiplier.max, &_multiplier.incr);
    _increment(&_factor._red, _color.min.red, 256, &_color.red_incr);
    _increment(&_factor._green, _color.min.green, 256, &_color.green_incr);
    _increment(&_factor._blue, _color.min.blue, 256, &_color.blue_incr);
}

// ------------------------------------------------------------------------
// Flashing Animation

FlashingAnimation::FlashingAnimation(ClockPlugin &clock, Color color, uint16_t time, uint8_t mod) : Animation(clock), _color(color), _time(time), _mod(mod)
{
    __LDBG_printf("color=%s time=%u", _color.toString().c_str(), _time);
}

void FlashingAnimation::begin()
{
    __LDBG_printf("begin color=%s time=%u", _color.toString().c_str(), _time);
    _finished = false;
    _blinkColon = false;
    setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {
        return (params.millis / _time) % _mod == 0 ? SevenSegmentDisplay::_adjustBrightnessAlways(_color, params.brightness) : 0;
    });
    setUpdateRate(std::max((uint16_t)(ClockPlugin::kMinFlashingSpeed / _mod), (uint16_t)(_time / _mod)));
}

void FlashingAnimation::end()
{
    __LDBG_printf("end color=%s time=%u", _color.toString().c_str(), _time);
    Animation::end();
}

// ------------------------------------------------------------------------
SkipRowsAnimation::SkipRowsAnimation(ClockPlugin &clock, uint16_t rows, uint16_t cols, uint32_t time) :
    Animation(clock),
    _rows(rows),
    _cols(cols),
    _time(time)
{
    setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {
        uint16_t start = _time ? params.millis / _time : 0;
        if (_rows > 1 && (address / IOT_LED_MATRIX_COLS / _rows) != (start % IOT_LED_MATRIX_ROWS)) {
            return 0;
        }
        if (_cols > 1 && (address % IOT_LED_MATRIX_COLS / _cols) != (start % IOT_LED_MATRIX_COLS)) {
            return 0;
        }
        return SevenSegmentDisplay::_adjustBrightnessAlways(color, params.brightness);
    });
    setUpdateRate(std::min<uint32_t>(std::max(5U, _time), ClockPlugin::kDefaultUpdateRate));
}

void SkipRowsAnimation::begin()
{
    __LDBG_printf("begin rows=%u cols=%u time=%u", _rows, _cols, _time);
}

void SkipRowsAnimation::end()
{
    __LDBG_printf("end rows=%u cols=%u time=%u", _rows, _cols, _time);
    Animation::end();
}

// ------------------------------------------------------------------------
// Fire Animation

FireAnimation::FireAnimation(ClockPlugin &clock, FireAnimationConfig &cfg) :
    Animation(clock),
    _lineCount((cfg.cast_enum_orientation(cfg.orientation) == Orientation::VERTICAL) ? IOT_LED_MATRIX_COLS : IOT_LED_MATRIX_ROWS),
    _lines(new Line[_lineCount]),
    _cfg(cfg)
{
    if (!_lines) {
        _lineCount = 0;
    }
    else {
        for(uint16_t i = 0; i < _lineCount; i++) {
            _lines[i].init((_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) ? IOT_LED_MATRIX_ROWS : IOT_LED_MATRIX_COLS);
        }
    }

}

FireAnimation::~FireAnimation()
{
    if (_lines) {
        delete[] _lines;
    }
}

void FireAnimation::begin()
{
    __LDBG_printf("begin lines=%u", _lineCount);
    setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {
        uint16_t row = address % IOT_LED_MATRIX_ROWS;
        uint16_t col = address / IOT_LED_MATRIX_ROWS;
        if (col % 2 != 0) {
            row = (IOT_LED_MATRIX_ROWS - 1) - row;
        }

        uint16_t line = (_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) ? col : row;
        if (line < _lineCount) {
            uint16_t pixel;
            if (_cfg.invert_direction) {
                pixel = (_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) ? IOT_LED_MATRIX_ROWS - row : IOT_LED_MATRIX_COLS - col;
            }
            else {
                pixel = (_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) ? row : col;
            }

            return SevenSegmentDisplay::_adjustBrightnessAlways(_lines[line].getHeatColor(pixel), params.brightness);
        }
        return 0;
    });
    setUpdateRate(std::max<uint16_t>(5, _cfg.speed));
}

void FireAnimation::end()
{
    __LDBG_printf("end lines=%u", _lineCount);
}


// ------------------------------------------------------------------------
// Callback Animation

CallbackAnimation::CallbackAnimation(ClockPlugin &clock, AnimationCallback callback, uint16_t updateRate, bool blinkColon) :
    Animation(clock),
    _callback(callback),
    _updateRate(updateRate),
    _blinkColon(blinkColon)
{
    __LDBG_printf("callback=%p/%u update_rate=%u blink_colon=%u", &_callback, (bool)_callback, _updateRate, _blinkColon);
}

void CallbackAnimation::begin()
{
    __LDBG_printf("begin");
    _finished = false;
    Animation::_blinkColon = _blinkColon;
    setUpdateRate(_updateRate);
    setAnimationCallback(_callback);
}

void CallbackAnimation::end()
{
    __LDBG_printf("end");
    Animation::end();
}
