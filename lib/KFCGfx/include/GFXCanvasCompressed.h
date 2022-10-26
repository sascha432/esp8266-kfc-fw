/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

/*

RLE compressed 16bit canvas

*/

#include <MicrosTimer.h>
#include <PrintString.h>
#include <Buffer.h>
#include <forward_list>
#include "AdafruitGFXExtension.h"
#include "GFXCanvasConfig.h"
#include "GFXCanvasStats.h"
#include "GFXCanvasLines.h"
#include "GFXCanvasBitmapStream.h"
#include "GFXPalette.h"

#if DEBUG_GFXCANVAS
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

using namespace GFXCanvas;

class GFXCanvasBitmapStream;

class GFXCanvasCompressed : public AdafruitGFXExtension {
public:
    typedef std::function<void(uXType x, uYType y, uWidthType width, ColorType *colorsPtr)> DrawLineCallback_t;

    static constexpr size_t kCachedLinesMax = GFXCANVAS_MAX_CACHED_LINES;

    GFXCanvasCompressed(uWidthType width, uHeightType height, ColorPalette *palette = nullptr);
    GFXCanvasCompressed(uWidthType width, const Lines &lines, ColorPalette *palette = nullptr);
    virtual ~GFXCanvasCompressed();

    ColorPalette *getColorPalette();

    virtual GFXCanvasCompressed *clone();

    virtual void setRotation(uint8_t r);

    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    #if HAVE_GFX_READLINE
        virtual void readLine(int16_t y, uint16_t *colors) {
            AdafruitGFXExtension::readLine(y, colors);
        }
    #endif
    virtual void fillScreen(uint16_t color);
    void fillScreenPartial(int16_t y, int16_t height, uint16_t color);
    virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color);
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t width, uint16_t color);

    void flushCache();
    void freeCache();

    // drawRGBBitmap() can be used in the callback. To get maximum performance, check if the TFT class provides a way to write a row at once
    //
    // following example is about 8x faster than drawRGBBitmap()
    //
    // Adafruit_ST7735 _tft;
    // _tft.startWrite();
    // _tft.setAddrWindow(0, 0, 128, 160);
    //
    // _canvas.drawInto(_tft, 0, 0, 128, 160);
    // ***OR***
    // _canvas.drawInto(0, 0, 128, 160, [_tft](uint16_t x, uint16_t y, uint16_t width, uint16_t *colorsPtr) {
    //     _tft.writePixels(colorsPtr, width);
    // });
    //
    // _tft.endWrite();

    bool _drawIntoClipping(sXType &x, sYType &y, sWidthType &width, sHeightType &height, sYType &xOfs, sYType &yOfs);

    template<class T>
    void drawInto(T &target, sXType x, sYType y, sWidthType width, sHeightType height) {
        __DBG_STATS(
            uint32_t start = micros();
        );
        sXType xOfs;
        sYType yOfs;
        if (_drawIntoClipping(x, y, width, height, xOfs, yOfs)) {
            y -= yOfs;
            height -= yOfs;

            for (; y < height; y++) {
                auto &cache = _decodeLine(y);
                auto ptr = cache.getBuffer() - xOfs;
                target.writePixels(ptr, width);
            }
        }

        _cache.release(*this);
        __DBG_STATS(
            stats.drawIntoCount++;
            stats.drawInto += micros() - start;
        );
    }

    template<class T>
    void drawInto(T &target) {
        drawInto(target, 0, 0, _width, _height);
    }

    void drawInto(DrawLineCallback_t callback);
    void drawInto(sXType x, sYType y, sWidthType width, sHeightType height, DrawLineCallback_t callback);

    virtual void getDetails(Print &output, bool displayPalette = false) const;

    GFXCanvasBitmapStream getBitmap();
    GFXCanvasBitmapStream getBitmap(uXType x, uYType y, uWidthType width, uHeightType height);

    Cache &getLine(sYType y);

private:
    friend GFXCanvasBitmapStream;
    friend GFXCanvas::SingleLineCache;

    // Cache &_getCache(sYType y);
    void _decodeLine(Cache &);
    Cache &_decodeLine(sYType y);
    void _encodeLine(Cache &cache);

protected:
    virtual void _RLEdecode(ByteBuffer &buffer, ColorType *output);
    virtual void _RLEencode(ColorType *data, Buffer &buffer);

protected:
    ColorPalette *_palette;
    Lines _lines;

private:
    LinesCache _cache;

    void clipX(sXType &startX, sXType &endX);
    void clipY(sXType &startY, sXType &endY);
};

inline ColorPalette *GFXCanvasCompressed::getColorPalette()
{
    return _palette;
}

inline void GFXCanvasCompressed::clipX(sXType &startX, sXType &endX)
{
    if (startX < 0) {
        startX = 0;
    }
    else if (startX >= _width) {
        startX = _width - 1;
    }
    if (endX < startX) {
        endX = startX;
    }
    else if (endX >= _width) {
        endX = _width - 1;
    }
}

inline void GFXCanvasCompressed::clipY(sXType &startY, sXType &endY)
{
    if (startY < 0) {
        startY = 0;
    }
    else if (startY >= _height) {
        startY = _height - 1;
    }
    if (endY < startY) {
        endY = startY;
    }
    else if (endY >= _height) {
        endY = _height - 1;
    }
}

inline void GFXCanvasCompressed::drawInto(DrawLineCallback_t callback)
{
    drawInto(0, 0, _width, _height, callback);
}

#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif
