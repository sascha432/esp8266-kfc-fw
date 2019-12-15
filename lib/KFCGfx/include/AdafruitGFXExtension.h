/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Adafruit_GFX.h"

class AdafruitGFXExtension : public Adafruit_GFX {
public:
	using Adafruit_GFX::Adafruit_GFX;

    typedef enum {
        LEFT,
        CENTER,
        RIGHT,
    } TextAlignEnum_t;

    typedef enum {
        TOP,
        MIDDLE,
        BOTTOM,
    } TextVAlignEnum_t;

    typedef struct {
        int16_t x;
        int16_t y;
        uint16_t w;
        uint16_t h;
    } Position_t;

    typedef struct {
        uint16_t w;
        uint16_t h;
    } Dimensions_t;

    void _drawTextAligned(int16_t x, int16_t y, const String& text, TextAlignEnum_t align = LEFT, TextVAlignEnum_t valign = TOP, Position_t* pos = nullptr);
    void _drawTextAligned(int16_t x, int16_t y, const char* text, TextAlignEnum_t align = LEFT, TextVAlignEnum_t valign = TOP, Position_t* pos = nullptr);

    // draw bitmaps with header, supports 2 bit/4 colors using the rgb565 colors passed in palette
    void _drawBitmap(int16_t x, int16_t y, PGM_P bmp, const uint16_t /*PROGMEM*/ *palette, Dimensions_t* dim = nullptr);
};
