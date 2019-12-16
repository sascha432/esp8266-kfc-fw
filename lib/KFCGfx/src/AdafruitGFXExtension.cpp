/**
 * Author: sascha_lammers@gmx.de
 */

#if HAVE_GFX_LIB

#include "AdafruitGFXExtension.h"

void AdafruitGFXExtension::_drawTextAligned(int16_t x, int16_t y, const String& text, TextAlignEnum_t align, TextVAlignEnum_t valign, Position_t* pos)
{
    _drawTextAligned(x, y, text.c_str(), align, valign, pos);
}

void AdafruitGFXExtension::_drawTextAligned(int16_t x, int16_t y, const char* text, TextAlignEnum_t align, TextVAlignEnum_t valign, Position_t* pos)
{
    int16_t x1, y1;
    uint16_t w, h;

    getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    y -= y1;

    switch (valign) {
    case BOTTOM:
        y -= h;
        break;
    case MIDDLE:
        y -= h / 2;
        break;
    case TOP:
    default:
        break;
    }
    switch (align) {
    case RIGHT:
        x -= w;
        break;
    case CENTER:
        x -= w / 2;
        break;
    case LEFT:
    default:
        break;
    }

    setCursor(x, y);
    print(text);

    if (pos) {
        pos->x = x;
        pos->y = y + y1;
        pos->w = w;
        pos->h = h;
    }

}

void AdafruitGFXExtension::_drawBitmap(int16_t x, int16_t y, PGM_P bmp, const uint16_t *palette, Dimensions_t* dim)
{
    uint16_t width = 0, height = 0;

    auto dataPtr = bmp + 1;
    if (pgm_read_byte(dataPtr++) == 2) { // only 2 bit depth supported
        width = (pgm_read_byte(dataPtr++) >> 8);
        width |= pgm_read_byte(dataPtr++);
        height = (pgm_read_byte(dataPtr++) >> 8);
        height |= pgm_read_byte(dataPtr++);

        // _debug_printf_P(PSTR("_drawBitmap(): %u (%u) x %u x %u\n"), width, ((width + 7) & ~7), height, pgm_read_byte(bmp + 1));

        width += x; // set to x end position
        uint8_t ofs = (x & 3); // first bit relative to x1, indicator for reading a new byte
        uint8_t byte = 0;
        while (height--) {
            for (uint16_t x1 = x; x1 < width; x1++) {
                if ((x1 & 3) == ofs) {
                    byte = pgm_read_byte(dataPtr++);
                    byte = (byte >> 6) | (((byte >> 4) & 0x3) << 2) | (((byte >> 2) & 0x3) << 4) | (((byte) & 0x3) << 6);   // invert pixel order
                }
                else {
                    byte >>= 2;
                }
                drawPixel(x1, y, pgm_read_word(&palette[byte & 0x3]));
            }
            y++;
        }
    }

    if (dim) {
        dim->w = width;
        dim->h = height;
    }
}

#endif
