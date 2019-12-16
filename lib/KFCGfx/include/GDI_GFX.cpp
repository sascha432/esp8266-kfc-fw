/**
* Author: sascha_lammers@gmx.de
*/

#ifdef _WIN32

#include "GDI_GFX.h"

GDI_GFX::GDI_GFX(uint16_t w, uint16_t h, HDC hdc) : Adafruit_GFX(w, h), _hdc(hdc) {
}

void GDI_GFX::drawPixel(int16_t x, int16_t y, uint16_t color) {
    SetPixel(_hdc, x, y, GFXCanvasCompressed::convertToRGB(color));
}

#endif
