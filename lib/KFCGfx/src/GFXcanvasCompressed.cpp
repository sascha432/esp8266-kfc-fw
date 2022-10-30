/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <Buffer.h>
#include "GFXCanvasConfig.h"

#include "GFXCanvas.h"
#include "GFXCanvasCompressed.h"
#include <algorithm>

#if ESP8266
extern "C" {
    #include "user_interface.h"
}
#endif

#pragma GCC push_options
#pragma GCC optimize("O3")

#if DEBUG_GFXCANVAS
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace GFXCanvas;

GFXCanvasCompressed::GFXCanvasCompressed(uWidthType width, uHeightType height) :
    AdafruitGFXExtension(width, height),
    _lines(height),
    _decodePalette(true),
    _cache(width, height, kCachedLinesMax)
{
}

GFXCanvasCompressed::GFXCanvasCompressed(uWidthType width, const Lines &lines) :
    AdafruitGFXExtension(width, lines.height()),
    _lines(lines),
    _decodePalette(true),
    _cache(width, _lines.height(), 0)
{
}

GFXCanvasCompressed *GFXCanvasCompressed::clone()
{
    return new GFXCanvasCompressed(width(), _lines);
}

void GFXCanvasCompressed::setRotation(uint8_t r)
{
    __DBG_panic("not allowed");
}

void GFXCanvasCompressed::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (isValidXY(x, y, _width, _height)) {
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(y, _height));
        auto &cache = _decodeLine(y);
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sx(x, _width));
        cache.setPixel(x, color);
        cache.setDirty(true);
    }
}

void GFXCanvasCompressed::fillScreen(uint16_t color)
{
    _lines.clear(color);
    _cache.release(*this);
}

void GFXCanvasCompressed::fillScreenPartial(sYType y, sHeightType height, ColorType color)
{
    auto endY = getClippedY(y + height, y, _height);
    __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(y, _height) || __DBG_BOUNDS_ey(endY, y, _height));
    _cache.invalidateRange(y, endY);
    _lines.fill(color, y, endY);
}

void GFXCanvasCompressed::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (isFullLine(x, x + w, _width)) {
        // use faster method fillscreen
        fillScreenPartial(y, h, color);
    }
    else {
        auto start = getClippedXY(x, y, _width, _height);
        auto end = getClippedXY(x + w, y + h, start.x, _width, start.y, _height);
        __DBG_BOUNDS_sx(start.x, _width);
        __DBG_BOUNDS_ex(end.x, start.x, _width);
        __DBG_BOUNDS_sy(start.y, _height);
        __DBG_BOUNDS_ey(end.y, start.y, _height);
        while (start.y < end.y) {
            __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(start.y, _height));
            auto &cache = _decodeLine(start.y);
            cache.setDirty(true);
            cache.fill(color, start.x, end.x);
            start.y++;
        }
    }
}

void GFXCanvasCompressed::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (isValidY(y, _height)) {
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(y, _height));
        sXType endX = x + w;
        if (isFullLine(x, endX, _width)) {
            auto &cache = _cache.get(*this, y);
            cache.setY(Cache::kYInvalid);
            _lines.fill(color, y);
        }
        else {
            __DBG_BOUNDS_sx(x, _width);
            __DBG_BOUNDS_ex(endX, x, _width);
            auto &cache = _decodeLine(y);
            cache.setDirty(true);
            cache.fill(color, x, endX);
        }
    }
}

bool GFXCanvasCompressed::_drawIntoClipping(sXType &x, sYType &y, sWidthType &w, sHeightType &h_in_y2_out, sXType &xOfs, sYType &yOfs)
{
    sXType x2;
    sYType y2;
    y2 = y + h_in_y2_out;
    if (y < 0) {
        yOfs = y;
        y = 0;
    }
    else {
        yOfs = 0;
    }
    if (y2 > _height + yOfs) {
        y2 = _height + yOfs;
    }
    if (y >= y2) {
        return false;
    }
    x2 = x + w;
    if (x < 0) {
        xOfs = x;
        x = 0;
    }
    else {
        xOfs = 0;
    }
    if (x2 > _width + xOfs) {
        x2 = _width + xOfs;
    }
    if (x >= x2) {
        return false;
    }
    w = x2 - x;
    h_in_y2_out = y2;
    return true;
}

void GFXCanvasCompressed::drawInto(sXType x, sYType y, sWidthType w, sHeightType h, DrawLineCallback_t callback)
{
    __DBG_STATS(
        uint32_t start = micros();
    );

    sXType xOfs;
    sYType yOfs;
    if (_drawIntoClipping(x, y, w, h, xOfs, yOfs)) {
        __DBG_BOUNDS_RETURN(
            __DBG_BOUNDS_sx(x, _width) ||
            __DBG_BOUNDS_ex(x + w, x, _width) ||
            __DBG_BOUNDS_sy(y, _height) ||
            __DBG_BOUNDS_ey(y + w, y, _height)
        );
        for(; y < h; y++) {
            __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(y - yOfs, _height));
            auto &cache = _decodeLine(y - yOfs);
            auto buffer = cache.getBuffer();
            auto ptr = &buffer[-xOfs];
            __DBG_BOUNDS_RETURN(__DBG_BOUNDS_buffer(ptr, buffer, &buffer[_width]));
            callback(x, y, w, ptr);
        }
    }

    _cache.release(*this);
    __DBG_STATS(
        stats.drawIntoCount++;
        stats.drawInto += micros() - start;
    );
}

void GFXCanvasCompressed::flushCache()
{
    _cache.flush(*this);
}

void GFXCanvasCompressed::freeCache()
{
    _cache.release(*this);
}

void GFXCanvasCompressed::_RLEdecode(ByteBuffer &buffer, ColorType *output)
{
    __DBG_BOUNDS(int count = 0);
    auto begin = buffer.begin();
    auto end = buffer.end() - 2; // we read 3 bytes each loop
    while(begin < end) {
        auto rle = *begin++;
        ColorType color = *begin++;
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_buffer_end(begin, buffer.end()));
        color |= (*begin++ << 8);
        __DBG_BOUNDS(count += rle);
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_buffer_end(&output[rle - 1], &output[_width]));
        output = std::fill_n(output, rle, color);
    }
    __DBG_BOUNDS_assert(count == _width);
}

void GFXCanvasCompressed::_RLEencode(ColorType *data, Buffer &buffer)
{
    uint8_t rle = 0;
    auto begin = data;
    auto end = &data[_width];
    auto lastColor = *data;
    __DBG_BOUNDS(int count = 0);

    while(begin < end) {
        auto color = *begin++;
        if (color == lastColor && rle != 0xff) {
            rle++;
        }
        else {
            __DBG_BOUNDS(count += rle);
            buffer.write(rle);
            buffer.write(static_cast<uint8_t>(lastColor));
            buffer.write(static_cast<uint8_t>(lastColor >> 8));
            lastColor = color;
            rle = 1;
        }
    }
    if (begin == end) {
        __DBG_BOUNDS(count += rle);
        buffer.write(rle);
        buffer.write(static_cast<uint8_t>(lastColor));
        buffer.write(static_cast<uint8_t>(lastColor >> 8));
    }
    else {
        __DBG_printf("begin=%p != end=%p rle=%u ???", begin, end, rle); //TODO remove
    }
    __DBG_BOUNDS_assert(count == _width);
}

void GFXCanvasCompressed::_decodeLine(Cache &cache)
{
    __DBG_BOUNDS_sy(cache.getY(), _height);
    auto &line = _lines.getLine(cache.getY());
    __DBG_BOUNDS_sy(cache.getY(), _height);
    if (line.length() == 0) {
        cache.fill(line.getFillColor());
    }
    else {
        _RLEdecode(line.getBuffer(), cache.getBuffer());
    }
    cache.setReadFlag(true);
}

Cache &GFXCanvasCompressed::_decodeLine(sYType y)
{
    __DBG_BOUNDS_sy(y, _height);
    __DBG_STATS(
        MicrosTimer timer;
        timer.start();
    );

    auto &cache = _cache.get(*this, y);
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert(cache.getY() == y), return cache);
    if (cache.hasReadFlag()) {
        __DBG_ASTATS(stats.cache_hits++);
        return cache;
    }
    else {
        __DBG_ASTATS(stats.cache_miss++);
    }
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert(cache.isDirty() == false), return cache);

    _decodeLine(cache);

    __DBG_STATS(
        stats.heap_low = std::min(stats.heap_low, ESP.getFreeHeap());
        stats.decode_time += timer.getTime() / 1000.0;
        stats.decode_count++;
    );
    return cache;
}

void GFXCanvasCompressed::_encodeLine(Cache &cache)
{
    __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(cache.getY(), _height));
    __DBG_STATS(
        MicrosTimer timer;
        timer.start();
    );

    auto &lineBuffer = _lines.getBuffer(cache.getY());
    Buffer buffer;
    _RLEencode(cache.getBuffer(),  buffer);
    lineBuffer = std::move(buffer);

    cache.setDirty(false);

    __DBG_STATS(
        stats.heap_low = std::min(stats.heap_low, ESP.getFreeHeap());
        stats.encode_time += timer.getTime() / 1000.0;
        stats.encode_count++;
    );
}

GFXCanvasBitmapStream GFXCanvasCompressed::getBitmap()
{
    __LDBG_printf("GFXCanvasBitmapStream(*this)");
    return GFXCanvasBitmapStream(*this);
}

GFXCanvasBitmapStream GFXCanvasCompressed::getBitmap(uXType x, uYType y, uWidthType width, uHeightType height)
{
    __LDBG_printf("GFXCanvasBitmapStream(*this, %d, %d, %d, %d)", x, y, width, height);
    return GFXCanvasBitmapStream(*this, x, y, width, height);
}

Cache &GFXCanvasCompressed::getLine(sYType y)
{
    return _decodeLine(y);
}

#pragma GCC pop_options
