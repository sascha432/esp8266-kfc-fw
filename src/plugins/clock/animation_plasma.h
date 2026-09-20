/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

    // ------------------------------------------------------------------------
    // PlasmaField

    // generates the plasma field, used by the plasma animation and the audio reactive visualizer
    // all pixels of the output are written

    class PlasmaField {
    public:
        struct ParamsType {
            float time;             // phase time, animation millis multiplied by the speed factor
            uint32_t hueShift;      // hue offset in 0-255
            float zoomX;            // multiplier for x_size, 1.0 does not change the plasma
            float zoomY;            // multiplier for y_size, 1.0 does not change the plasma
            const uint8_t *bands;   // 32 spectrum values or nullptr
            uint8_t bandGain;       // 0-255, 0 disables the spectrum modulation

            ParamsType(float _time = 0.0f, uint32_t _hueShift = 0, float _zoomX = 1.0f, float _zoomY = 1.0f, const uint8_t *_bands = nullptr, uint8_t _bandGain = 0) :
                time(_time),
                hueShift(_hueShift),
                zoomX(_zoomX),
                zoomY(_zoomY),
                bands(_bands),
                bandGain(_bandGain)
            {
            }
        };

    public:
        // _Tc requires the members angle1, angle2, angle3, angle4, x_size and y_size
        template<typename _Ta, typename _Tc>
        static void copyTo(_Ta &output, const _Tc &cfg, const ParamsType &params)
        {
            // the first two layers use x_size, the last two y_size
            float sizeX1 = cfg.x_size * params.zoomX;
            float sizeX2 = sizeX1;
            float sizeY1 = cfg.y_size * params.zoomY;
            float sizeY2 = sizeY1;

            if (params.bands && params.bandGain) {
                // each layer is modulated by a different part of the spectrum: bass, low mid, high mid, treble
                const float scale = params.bandGain / 255.0f;
                sizeX1 *= 1.0f + _getBandLevel(params.bands, 0) * scale;
                sizeX2 *= 1.0f + _getBandLevel(params.bands, 1) * scale;
                sizeY1 *= 1.0f + _getBandLevel(params.bands, 2) * scale;
                sizeY2 *= 1.0f + _getBandLevel(params.bands, 3) * scale;
            }

            const float t = params.time;
            const float angle1 = cfg.angle1 * t;
            const float angle2 = cfg.angle2 * t;
            const float angle3 = cfg.angle3 * t;
            const float angle4 = cfg.angle4 * t;
            const uint32_t hueShift = params.hueShift;

            float x1, x2, x3, x4, y1, y2, y3, y4, sx1, sx2, sx3, sx4;
            sx1 = cos(angle1) * radius1 + centerX1;
            sx2 = cos(angle2) * radius2 + centerX2;
            sx3 = cos(angle3) * radius3 + centerX3;
            sx4 = cos(angle4) * radius4 + centerX4;
            y1 = sin(angle1) * radius1 + centerY1;
            y2 = sin(angle2) * radius2 + centerY2;
            y3 = sin(angle3) * radius3 + centerY3;
            y4 = sin(angle4) * radius4 + centerY4;

            CHSV color(0, 255, 255);

            for(CoordinateType y = 0; y < output.getRows(); y++) {
                x1 = sx1;
                x2 = sx2;
                x3 = sx3;
                x4 = sx4;
                for(CoordinateType x = 0; x < output.getCols(); x++) {
                    uint32_t value = hueShift
                        + _readSineTab((x1 * x1 + y1 * y1) / sizeX1)
                        + _readSineTab((x2 * x2 + y2 * y2) / sizeX2)
                        + _readSineTab((x3 * x3 + y3 * y3) / sizeY1)
                        + _readSineTab((x4 * x4 + y4 * y4) / sizeY2);

                    color.hue = value / 4;
                    output.setPixel(PixelCoordinatesType(y, x), color);

                    x1--;
                    x2--;
                    x3--;
                    x4--;
                }
                y1--;
                y2--;
                y3--;
                y4--;
            }
        }

    private:
        // average of 8 spectrum values, normalized to 0.0-1.0
        static float _getBandLevel(const uint8_t *bands, uint8_t group)
        {
            uint16_t sum = 0;
            for(uint8_t i = 0; i < 8; i++) {
                sum += bands[group * 8 + i];
            }
            return sum / (8.0f * 255.0f);
        }

        static int8_t _readSineTab(uint8_t ofs);

        static constexpr float radius1 = 65.2, radius2 = 92.0, radius3 = 163.2, radius4 = 176.8, centerX1 = 64.4, centerX2 = 46.4, centerX3 = 93.6, centerX4 = 16.4, centerY1 = 34.8, centerY2 = 26.0, centerY3 = 56.0, centerY4 = -11.6;
    };

    // ------------------------------------------------------------------------
    // PlasmaAnimation

    class PlasmaAnimation : public Animation {
    public:
        using PlasmaAnimationConfig = KFCConfigurationClasses::Plugins::ClockConfigNS::PlasmaAnimationType;

    public:
        PlasmaAnimation(ClockPlugin &clock, Color color, PlasmaAnimationConfig &cfg) :
            Animation(clock, color),
            _cfg(cfg)
        {
            _disableBlinkColon = false;
        }

        virtual void copyTo(DisplayType &display, uint32_t millisValue) override
        {
            _copyTo(display, millisValue);
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) override
        {
            _copyTo(buffer, millisValue);
        }

        template<typename _Ta>
        void _copyTo(_Ta &output, uint32_t millisValue)
        {
            float t = millisValue * (_cfg.speed * ((1.0 / (192.0 * 100000.0))));
            uint32_t hueShift = _cfg.hue_shift / ((millisValue >> 14) + 1); // limit hue_shift to 16.384 per second

            PlasmaField::copyTo(output, _cfg, PlasmaField::ParamsType(t, hueShift));
        }

    private:
        PlasmaAnimationConfig &_cfg;
    };

}
