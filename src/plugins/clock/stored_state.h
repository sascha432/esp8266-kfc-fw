/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "animation.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

#if IOT_LED_MATRIX
#define IF_IOT_LED_MATRIX(...)      __VA_ARGS__
#define IF_IOT_CLOCK(...)
#else
#define IF_IOT_LED_MATRIX(...)
#define IF_IOT_CLOCK(...)           __VA_ARGS__
#endif


namespace Clock {

    class StoredState {
    public:
        using ClockColor_t = Plugins::ClockConfig::ClockColor_t;

        typedef struct __attribute__packed__ {
            uint16_t _crc16;
            BrightnessType _brightness;
            AnimationType _animation;
            ClockColor_t _color;
            IF_IOT_CLOCK(uint16_t _blink_colon_speed);
        } FileFormat_t;

    public:

        StoredState() : _storage()
        {}

        StoredState(BrightnessType brightness, AnimationType animation, Color color IF_IOT_CLOCK(, uint16_t blink_colon_speed)) :
            _storage({0, brightness, animation, color.get() IF_IOT_CLOCK(, blink_colon_speed)})
        {
            _updateCrc();
        }

        bool hasValidData() const {
            return _storage._crc16 != 0;
        }
        void clear() {
            _storage = {};
        }

        bool operator==(const StoredState &state) const {
            return _getCrc() == state._getCrc();
        }
        bool operator!=(const StoredState &state) const {
            return !operator==(state);
        }

        void setBrightness(BrightnessType brightness) {
            _storage._brightness = brightness;
        }
        BrightnessType getBrightness() const {
            return _storage._brightness;
        }

        void setAnimation(uint8_t animation) {
            _storage._animation = static_cast<AnimationType>(animation);
        }
        void setAnimation(AnimationType animation) {
            _storage._animation = animation;
        }

        AnimationType getAnimation() const {
            return _storage._animation;
        }
        uint8_t getAnimationInt() const {
            return static_cast<uint8_t>(_storage._animation);
        }

        void setColor(uint32_t color) {
            _storage._color = color;
        }
        void setColor(Color color) {
            _storage._color = color.get();
        }

        Color getColor() const {
            return Color(_storage._color.value);
        }

#if !IOT_LED_MATRIX
        void setBlinkColonSpeed(uint16_t blink_colon_speed) {
            _storage._blink_colon_speed = blink_colon_speed;
        }

        uint16_t getBlinkColonSpeed() const{
            return _storage._blink_colon_speed;
        }
#endif

        bool store(Stream &stream) {
            _updateCrc();
            return (stream.write(reinterpret_cast<const uint8_t *>(&_storage), sizeof(_storage)) == sizeof(_storage));
        }

        static StoredState load(Stream &stream) {
            StoredState state;
            if (
                (stream.readBytes(reinterpret_cast<uint8_t *>(&state._storage), sizeof(state._storage)) != sizeof(state._storage)) ||
                (state._storage._crc16 != _getCrc(state._storage))
            ) {
                state.clear();
            }
            return state;
        }

    private:
        static uint16_t _getCrc(const FileFormat_t &storage) {
            return crc16_update(&storage._brightness, sizeof(storage) - offsetof(FileFormat_t, _brightness));
        }

        uint16_t _getCrc() const {
            return _getCrc(_storage);
        }

        uint16_t _updateCrc() {
            return (_storage._crc16 == _getCrc());
        }

    private:
        FileFormat_t _storage;
    };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
