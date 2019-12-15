/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifdef _WIN32

#include <Adafruit_GFX.h>
#include <windows.h>

class GDI_GFX : public Adafruit_GFX {
public:
    GDI_GFX(uint16_t w, uint16_t h);
    virtual ~GDI_GFX();

    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);

    // draws buffered image to device context
    void drawBitmap(HDC dc, uint16_t x, uint16_t y);

private:
    uint8_t *_buffer;
    HBITMAP _hBitmap;
};

#endif
