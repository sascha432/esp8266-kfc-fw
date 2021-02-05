// /**
//  * Author: sascha_lammers@gmx.de
//  */

// #pragma once

// #include <Arduino_compat.h>
// #include "color.h"
// #include "pixel_display.h"

// #if DEBUG_IOT_CLOCK
// #include <debug_helper_enable.h>
// #else
// #include <debug_helper_disable.h>
// #endif

// namespace Clock {

//     class Renderer {
//     public:
//         enum class AnimationType {
//             CURRENT,
//             BLEND,
//         };

//     public:
//         Renderer(ClockPlugin &clock, AnimationType type);

//         Renderer(Renderer &&move) :
//             _clock(std::exchange(move._clock, nullptr)),
//             _type(move._type),
//             _callback(std::move(move._callback)),
//             _color(move._color),
//             _updateRate(move._updateRate),
//             _lastUpdate(move._lastUpdate)
//         {
//         }

//         Renderer &operator=(Renderer &&move) {
//             _clock = std::exchange(move._clock, nullptr);
//             _type = move._type;
//             _callback = std::move(move._callback);
//             _color = move._color;
//             _updateRate = move._updateRate;
//             _lastUpdate = move._lastUpdate;
//             return *this;
//         }

//         void setColor(Color color);
//         Color getColor() const;

//         void setUpdateRate(uint16_t updateRate);
//         uint16_t getUpdateRate() const;

//         void setAnimationCallback(AnimationCallback callback);

//     // private:
//         ClockPlugin *_clock;
//         AnimationType _type;
//         AnimationCallback _callback;
//         Color _color;
//         uint16_t _updateRate;
//         uint32_t _lastUpdate;

//     };

// }

// #if DEBUG_IOT_CLOCK
// #include <debug_helper_disable.h>
// #endif
