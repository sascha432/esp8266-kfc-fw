/**
 * Author: sascha_lammers@gmx.de
 */

#include <push_optimize.h>
#pragma GCC optimize ("O3")

#include "AdafruitGFXExtension.h"

uint8_t AdafruitGFXExtension::getFontHeight(const GFXfont *f) const
{
    return pgm_read_byte(&f->yAdvance);
}

void AdafruitGFXExtension::drawTextAligned(int16_t x, int16_t y, const String &text, TextAlignEnum_t align, TextVAlignEnum_t valign, Position_t *pos)
{
    drawTextAligned(x, y, text.c_str(), align, valign, pos);
}

void AdafruitGFXExtension::drawTextAligned(int16_t x, int16_t y, const char *text, TextAlignEnum_t align, TextVAlignEnum_t valign, Position_t *pos)
{
    int16_t x1, y1, sy;
    uint16_t w, h, lineH;
    auto sx = x;

    int lines = 0;
    auto ptr = text;
    while (*ptr) {
        if (*ptr++ == '\n') {
            lines++;
        }
    }
    lineH = (int16_t)textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
    if (lines) {
        if (valign == BOTTOM) {
            y += lines * lineH;
        }
        else {
            y -= ((lines * lineH) / 2);
        }
    }
    y -= lineH;

    int16_t maxX = 0, maxY = 0;
    int16_t minX = 0x7fff, minY = 0x7fff;

    auto buf = strdup(text);
    auto bufPtr = buf;
    char *endPos;
    do {
        auto line = bufPtr;
        endPos = strchr(bufPtr, '\n');
        if (endPos) {
            *endPos++ = 0;
            bufPtr = endPos;
        }

        getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
        sy = y;
        y += lineH;

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
        maxX = std::max(maxX, (int16_t)(x + w));
        maxY = std::max(maxY, (int16_t)(y + h));
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        y -= y1;

        setCursor(x, y);
        print(line);

        x = sx;
        y = sy + lineH;

    } while (endPos);


    if (pos) {
        pos->x = minX;
        pos->y = minY;
        pos->w = maxX - minX;
        pos->h = maxY - minY;
    }

    free(buf);
}

void AdafruitGFXExtension::drawBitmap(int16_t x, int16_t y, PGM_P bmp, const uint16_t *palette, Dimensions_t *dim)
{
    uint16_t width = 0, height = 0;

    auto dataPtr = bmp + 1;
    if (pgm_read_byte(dataPtr++) == 2) { // only 2 bit depth supported
        width = (pgm_read_byte(dataPtr++) >> 8);
        width |= pgm_read_byte(dataPtr++);
        height = (pgm_read_byte(dataPtr++) >> 8);
        height |= pgm_read_byte(dataPtr++);

        // _debug_printf_P(PSTR("drawBitmap(): %u (%u) x %u x %u\n"), width, ((width + 7) & ~7), height, pgm_read_byte(bmp + 1));

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

#include <pop_optimize.h>
