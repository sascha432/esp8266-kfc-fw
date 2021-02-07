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


Color Animation::_getColor() const
{
    return _parent._getColor();
}

void Animation::_setColor(Color color)
{
    _parent._setColor(color, false);
}

// Renderer::Renderer(ClockPlugin &clock, AnimationType type) :
//     _clock(&clock),
//     _type(type),
//     _color(clock.getColor()),
//     _updateRate(clock.getUpdateRate())
// {
// }

// void Renderer::setColor(Color color)
// {
//     // if (_type == AnimationType::CURRENT) {
//         // _clock->setColor(color);
//     // }
//     // else {
//     //     _color = color;
//     // }
//     _color = color;
// }

// Color Renderer::getColor() const
// {
//     // if (_type == AnimationType::CURRENT) {
//     //     return _clock->getColor();
//     // }
//     // else {
//         return _color;
//     // }
// }

// void Renderer::setUpdateRate(uint16_t updateRate)
// {
//     if (_type == AnimationType::CURRENT) {
//         if (_clock) {
//             _clock->setUpdateRate(updateRate);
//         }
//     }
//     else {
//         _updateRate = updateRate;
//     }
// }

// uint16_t Renderer::getUpdateRate() const
// {
//     if (_type == AnimationType::CURRENT) {
//         return _clock->getUpdateRate();
//     }
//     else {
//         return _updateRate;
//     }
// }

// void Renderer::setAnimationCallback(AnimationCallback callback)
// {
//     // if (_type == AnimationType::CURRENT) {
//     //     if (_clock) {
//     //         _clock->setAnimationCallback(callback);
//     //     }
//     // }
//     // else {
//     _callback = callback;
//     // }
// }


// ------------------------------------------------------------------------
// Base Class Animation

// Animation::Animation(ClockPlugin &clock, Renderer::AnimationType type) :
//     _renderer(std::move(Renderer(clock, type))),
//     _updateRate(clock.getUpdateRate()),
//     _finished(false),
//     _blinkColon(true)
// {
// }

// Animation::~Animation()
// {
// }

// void Animation::setUpdateRate(uint16_t updateRate)
// {
//     _updateRate = updateRate;
//     _renderer.setUpdateRate(_updateRate);
// }

// uint16_t Animation::getClockUpdateRate() const
// {
//     return _renderer.getUpdateRate();
// }


// void Animation::setColor(Color color)
// {
//     _renderer.setColor(color);
// }

// void Animation::setAnimationCallback(AnimationCallback callback)
// {
//     _renderer.setAnimationCallback(callback);
// }

// void Animation::removeAnimationCallback()
// {
//     _renderer.setAnimationCallback(nullptr);
// }

// void Animation::setLoopCallback(LoopCallback callback)
// {
//     _callback = callback;
// }

// void Animation::removeLoopCallback()
// {
//     _callback = nullptr;
// }

// ------------------------------------------------------------------------
// SolidColor Animation

// ...

// ------------------------------------------------------------------------
// Fading Animation

// FadingAnimation::FadingAnimation(ClockPlugin &clock, Color from, Color to, float speed, uint32_t time, Color factor) :
//     Animation(clock),
//     _from(from),
//     _to(to),
//     _time(time),
//     _speed(_secondsToSpeed(speed)),
//     _factor(factor)

// {
//     __LDBG_printf("from=%s to=%s time=%u speed=%u (%f) factor=%s", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, speed, _factor.toString().c_str());
// }

// void FadingAnimation::begin()
// {
//     _progress = 0;
//     setUpdateRate(kUpdateRate);
//     __LDBG_printf("begin from=%s to=%s time=%u speed=%u update_rate=%u", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, getUpdateRate());

//     setLoopCallback([this](uint32_t millisValue) {
//         this->callback(millisValue);
//         // no code here, the lambda function might be destroyed already
//     });
// }


// void FadingAnimation::callback(uint32_t millisValue)
// {
//     srand(millisValue);
//     if (_progress < kMaxProgress) {
//         _progress = std::min<uint32_t>(kMaxProgress, _progress + _speed);
//         float stepRed = (_to.red() - _from.red()) / kFloatMaxProgress;
//         float stepGreen = (_to.green() - _from.green()) / kFloatMaxProgress;
//         float stepBlue = (_to.blue() - _from.blue()) / kFloatMaxProgress;
//         setColor(Color(
//             _from.red() + static_cast<uint8_t>(stepRed * _progress),
//             _from.green() + static_cast<uint8_t>(stepGreen * _progress),
//             _from.blue() + static_cast<uint8_t>(stepBlue * _progress)
//         ));
//         if (_progress >= kMaxProgress) {
//             setColor(_to);
//             if (_time == kNoColorChange) {
//                 _finished = true;
//                 __LDBG_printf("end no color change");
//                 removeLoopCallback();
//                 return; // make sure to return after destroying the lambda function
//             }
//             else if (_time == 0) {
//                 _from = _to;
//                 _to.rnd();
//                 _progress = 0;
//                 __LDBG_printf("next from=%s to=%s time=%u speed=%u update_rate=%u", _from.toString().c_str(), _to.toString().c_str(), _time, _speed, getUpdateRate());
//             } else {
//                 // wait for _time seconds
//                 uint32_t endTime = millisValue + _time;
//                 __LDBG_printf("delay=%u time=%u", _time, endTime);
//                 _callback = [this, endTime](uint32_t millisValue) {
//                     if (millisValue > endTime) {
//                         // get current color and a random new color
//                         _from = getColor();
//                         _to.rnd();
//                         begin();
//                     }
//                 };
//             }
//         }
//     }
// }

// ------------------------------------------------------------------------
// Rainbow Animation

// RainbowAnimation::RainbowAnimation(ClockPlugin &clock, uint16_t speed, RainbowMultiplier multiplier, RainbowColor color) :
//     Animation(clock),
//     _speed(speed),
//     _multiplier(multiplier),
//     _color(color),
//     _factor(color.factor.value),
//     _lastUpdate(0)
// {
//     // __LDBG_printf("speed=%u _multiplier=%f multiplier=%f factor=%s mins=%s", _speed, _multiplier, multiplier, _factor.toString().c_str(), _minimum.toString().c_str());
// }

// void RainbowAnimation::_increment(float *valuePtr, float min, float max, float *incrPtr)
// {
//     float value;
//     float incr;
//     // we need to use memcpy in case the pointer is not dword aligned
//     memcpy(&value, valuePtr, sizeof(value));
//     memcpy(&incr, incrPtr, sizeof(incr));
//     if (incr) {
//         value += incr;
//         if (value < min && incr < 0) {
//             value = min;
//             incr = -incr;
//         }
//         else if (value >= max && incr > 0) {
//             incr = -incr;
//             value = max + incr;
//         }
//     }
//     memcpy(valuePtr, &value, sizeof(*valuePtr));
//     memcpy(incrPtr, &incr, sizeof(*incrPtr));
// }

// void RainbowAnimation::begin()
// {
//     _finished = false;
//     setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {

//         uint32_t ind = (address * _multiplier.value) + (params.millis / _speed);
//         uint8_t indMod = (ind % _mod);
//         uint8_t idx = (indMod / _divMul);
//         float factor1 = 1.0f - ((float)(indMod - (idx * _divMul)) / _divMul);
//         float factor2 = (float)((int)(ind - (idx * _divMul)) % _mod) / _divMul;

//         switch(idx) {
//             case 0:
//                 color = _normalizeColor(_factor.red() * factor1, _factor.green() * factor2, 0);
//                 break;
//             case 1:
//                 color = _normalizeColor(0, _factor.green() * factor1, _factor.blue() * factor2);
//                 break;
//             case 2:
//                 color = _normalizeColor(_factor.red() * factor2, 0, _factor.blue() * factor1);
//                 break;
//         }
//         return SevenSegmentDisplay::_adjustBrightnessAlways(color, params.brightness());
//     });
//     setUpdateRate(25);
//     __LDBG_printf("begin rate=%u", getUpdateRate());
// }

// void RainbowAnimation::end()
// {
//     __LDBG_printf("end rate=%u", getUpdateRate());
//     Animation::end();
// }

// void RainbowAnimation::loop(uint32_t millisValue)
// {
//     if (_lastUpdate == 0) {
//         _lastUpdate = millisValue;
//         return;
//     }
//     if (get_time_diff(_lastUpdate, millisValue) < 25) {
//         return;
//     }

//     _increment(&_multiplier.value, _multiplier.min, _multiplier.max, &_multiplier.incr);
//     _increment(&_factor._red, _color.min.red, 256, &_color.red_incr);
//     _increment(&_factor._green, _color.min.green, 256, &_color.green_incr);
//     _increment(&_factor._blue, _color.min.blue, 256, &_color.blue_incr);
// }

// ------------------------------------------------------------------------
// Flashing Animation


// void FlashingAnimation::begin()
// {
//     __LDBG_printf("begin color=%s time=%u", _color.toString().c_str(), _time);
//     _blinkColon = false;
//     setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {
//         return (params.millis / _time) % _mod == 0 ? SevenSegmentDisplay::_adjustBrightnessAlways(_color, params.brightness()) : 0;
//     });
//     setUpdateRate(std::max((uint16_t)(ClockPlugin::kMinFlashingSpeed / _mod), (uint16_t)(_time / _mod)));
// }

// void FlashingAnimation::end()
// {
//     __LDBG_printf("end color=%s time=%u", _color.toString().c_str(), _time);
//     Animation::end();
// }

// ------------------------------------------------------------------------
// InterleavedAnimation::InterleavedAnimation(ClockPlugin &clock, uint16_t row, uint16_t col, uint32_t time) :
//     Animation(clock),
//     _time(time),
//     _row(row),
//     _col(col)
// {
//     setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {
//         auto pixel = translateAddress(address);
//         if (_time) {
//             uint32_t position = params.millis / _time;
//             if (_row > 1 && (pixel.getRow() % _row) != (position % _row)) {
//                 return 0;
//             }
//             if (_col > 1 && (pixel.getCol() % _col) != (position % _col)) {
//                 return 0;
//             }
//         }
//         else {
//             if (_row > 1 && (pixel.getRow() % _row) != 0) {
//                 return 0;
//             }
//             if (_col > 1 && (pixel.getCol() % _col) != 0) {
//                 return 0;
//             }

//         }
//         return SevenSegmentDisplay::_adjustBrightnessAlways(color, params.brightness());
//     });
//     setUpdateRate(std::min<uint32_t>(std::max(5U, _time), ClockPlugin::kDefaultUpdateRate));
// }

// void InterleavedAnimation::begin()
// {
//     __LDBG_printf("begin rows=%u cols=%u time=%u", rows(), cols(), _time);
// }

// void InterleavedAnimation::end()
// {
//     __LDBG_printf("end rows=%u cols=%u time=%u", rows(), cols(), _time);
//     Animation::end();
// }

// ------------------------------------------------------------------------
// Fire Animation

// FireAnimation::FireAnimation(ClockPlugin &clock, FireAnimationConfig &cfg) :
//     Animation(clock),
//     _lineCount((cfg.cast_enum_orientation(cfg.orientation) == Orientation::VERTICAL) ? cols() : rows()),
//     _lines(new Line[_lineCount]),
//     _cfg(cfg)
// {
//     if (!_lines) {
//         _lineCount = 0;
//     }
//     else {
//         for(uint16_t i = 0; i < _lineCount; i++) {
//             _lines[i].init((_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) ? rows() : cols());
//         }
//     }
// }

// // ------------------------------------------------------------------------
// // Callback Animation

// CallbackAnimation::CallbackAnimation(ClockPlugin &clock, AnimationCallback callback, uint16_t updateRate, bool blinkColon) :
//     Animation(clock),
//     _callback(callback),
//     _updateRate(updateRate),
//     _blinkColon(blinkColon)
// {
//     __LDBG_printf("callback=%p/%u update_rate=%u blink_colon=%u", &_callback, (bool)_callback, _updateRate, _blinkColon);
// }

// void CallbackAnimation::begin()
// {
//     __LDBG_printf("begin");
//     _finished = false;
//     Animation::_blinkColon = _blinkColon;
//     setUpdateRate(_updateRate);
//     setAnimationCallback(_callback);
// }

// void CallbackAnimation::end()
// {
//     __LDBG_printf("end");
//     Animation::end();
// }
