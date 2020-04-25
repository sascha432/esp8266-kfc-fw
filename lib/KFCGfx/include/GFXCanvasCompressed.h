/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

/*

RLE compressed 16bit canvas

*/

#include <MicrosTimer.h>
#include <PrintString.h>
#include <forward_list>
#include "AdafruitGFXExtension.h"
#include "GFXCanvas.h"
#include "GFXCanvasBitmapStream.h"
#include "GFXCanvasRLEStream.h"

using namespace GFXCanvas;

class GFXCanvasCompressed : public AdafruitGFXExtension {
public:
    using ByteBuffer = GFXCanvas::ByteBuffer;
    typedef std::function<void(coord_x_t x, coord_y_t y, coord_x_t w, color_t *pcolors)> DrawLineCallback_t;

    GFXCanvasCompressed(coord_x_t w, coord_y_t h);
    virtual ~GFXCanvasCompressed();

    virtual GFXCanvasCompressed *clone();

    virtual void setRotation(uint8_t r);

    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void fillScreen(uint16_t color);
    void fillScreenPartial(int16_t y, int16_t height, uint16_t color);
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

    // increasing the number of cached lines can increase or decrease the performance depending on how the screen is drawn
    // use freeLineCache() to release all the cache after drawInto() / drawing
    //
    // GFXCANVAS_MAX_CACHED_LINES=1 uses a single cached line and frees the memory after drawInto()
    //
#if !(GFXCANVAS_MAX_CACHED_LINES < 2)
    void setMaxCachedLines(coord_y_t max);
#endif
    void flushLineCache();
    void freeLineCache();

    // drawRGBBitmap() can be used in the callback. To get maximum performance, check if the TFT class provides a way to write a row at once
    //
    // following example is about 8x faster than drawRGBBitmap()
    //
    // Adafruit_ST7735 _tft;
    // _tft.startWrite();
    // _tft.setAddrWindow(0, 0, 128, 160);
    // _canvas.drawInto(0, 0, 128, 160, [_tft](uint16_t x, uint16_t y, uint16_t w, uint16_t *pcolors) {
    //     _tft.writePixels(pcolors, w);
    // });
    // _tft.endWrite();

    void drawInto(scoord_x_t x, scoord_y_t y, scoord_x_t w, scoord_y_t h, DrawLineCallback_t callback);
    void drawInto(DrawLineCallback_t callback);

    virtual String getDetails() const;

    GFXCanvasBitmapStream getBitmap();
    GFXCanvasBitmapStream getBitmap(coord_x_t x, coord_y_t y, coord_x_t w, coord_y_t h);
    GFXCanvasRLEStream getRLEStream();
    GFXCanvasRLEStream getRLEStream(coord_x_t x, coord_y_t y, coord_x_t w, coord_y_t h);

    Cache &getLine(scoord_y_t y);

private:
    friend class GFXCanvasBitmapStream;
    friend class GFXCanvasRLEStream;

    Cache &_getCache(scoord_y_t y);
    void _decodeLine(Cache &);
    Cache &_decodeLine(scoord_y_t y);
    void _encodeLine(Cache &cache);

protected:
    virtual void _RLEdecode(ByteBuffer &buffer, color_t *output);
    virtual void _RLEencode(color_t *data, ByteBuffer &buffer);

    // colors are translated only inside the decode/encode methods
    virtual color_t getColor(color_t color, bool addIfNotExists = true);
    virtual color_t *getPalette(uint8_t &count);
    virtual void setPalette(color_t *palette, uint8_t count);

private:
    inline void limitX(int16_t &x1, int16_t x2) {
        if (x1 < 0) {
            x1 = 0;
        }
        if (x2 >= _height) {
            x2 = _height - 1;
        }
    }
    inline void limitY(int16_t &y1, int16_t y2) {
        if (y1 < 0) {
            y1 = 0;
        }
        if (y2 >= _height) {
            y2 = _height - 1;
        }
    }

protected:
    LineBuffer *_lineBuffer;

private:
#if GFXCANVAS_MAX_CACHED_LINES < 2
    Cache _cache;
#else
    typedef std::forward_list<Cache> CacheForwardList;
    typedef CacheForwardList::iterator CacheForwardListIterator;
    CacheForwardList _caches;
    coord_y_t _maxCachedLines;
#endif

#if DEBUG_GFXCANVASCOMPRESSED_STATS
public:
    void clearStats() {
        auto tmp = stats.cache_max;
        stats = Stats();
        stats.cache_max = tmp;
    }
protected:
    Stats stats;
#endif
};
