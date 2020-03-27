/**
 * Author: sascha_lammers@gmx.de
 */

#include "GFXCanvasCompressed.h"
#include <algorithm>
#if ESP8266
extern "C" {
    #include "user_interface.h"
}
#endif

//#include "Adafruit_ST7735.h"

#if DEBUG_GFXCANVASCOMPRESSED_STATS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace GFXCanvas;

GFXCanvasCompressed::GFXCanvasCompressed(uint16_t w, uint16_t h) :
    AdafruitGFXExtension(w, h),
#if GFXCANVAS_MAX_CACHED_LINES < 2
    _cache(w)
#else
    _maxCachedLines(GFXCANVAS_MAX_CACHED_LINES)
#endif
{
    _lineBuffer = new LineBuffer[h];
}

GFXCanvasCompressed::~GFXCanvasCompressed()
{
    if (_lineBuffer) {
        delete []_lineBuffer;
    }
}

GFXCanvasCompressed *GFXCanvasCompressed::clone()
{
    GFXCanvasCompressed* target = new GFXCanvasCompressed(width(), height());
    for (uint16_t y = 0; y < _height; y++) {
        target->_lineBuffer[y].clone(_lineBuffer[y]);
    }
    return target;
}

void GFXCanvasCompressed::setRotation(uint8_t r)
{
    __debugbreak_and_panic_printf_P(PSTR("GFXCanvasCompressed::setRotation() not allowed\n"));
}

void GFXCanvasCompressed::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if ((uint16_t)x < (uint16_t)_width && (uint16_t)y < (uint16_t)_height) {
        auto &cache = _decodeLine(y);
        cache.setPixel(x, color);
        cache.setWriteFlag(true);
    }
}

void GFXCanvasCompressed::fillScreen(uint16_t color)
{
    auto count = _height;
    while(count--) {
        _lineBuffer[count].clear(color);
    }
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.malloc += _height;
#endif
    freeLineCache();
}

void GFXCanvasCompressed::fillScreenPartial(int16_t y, uint16_t height, uint16_t color)
{
    auto endY = y + height;
    limitY(y, endY);
#if GFXCANVAS_MAX_CACHED_LINES < 2
    _cache.setY(Cache::INVALID);
#else
    for(auto &cache : _caches) {
        auto cy = cache.getY();
        if (cy >= y && cy < endY) {
            cache.setY(Cache::INVALID);
        }
    }
#endif
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.malloc += endY - y;
#endif
    while (y < endY) {
        _lineBuffer[y++].clear(color);
    }
}

void GFXCanvasCompressed::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x <= 0 && x + w >= _width) {
        // use faster method fillscreen
        fillScreenPartial(y, h, color);
    }
    else {
        int16_t x2 = x + w;
        int16_t y2 = y + h;
        limitX(x, x2);
        limitY(y, y2);
        while (y < y2) {
            int16_t x1 = x;
            auto& cache = _decodeLine(y);
            cache.setWriteFlag(true);
            while (x1 < x2) {
                cache.setPixel(x1, color);
                x1++;
            }
            y++;
        }
    }
}

void GFXCanvasCompressed::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if ((uint16_t)y < (uint16_t)_height) {
        if (x <= 0 && x + w >= _width) {
            auto &cache = _getCache(y);
            cache.setY(Cache::INVALID);
#if DEBUG_GFXCANVASCOMPRESSED_STATS
            stats.malloc++;
#endif
            _lineBuffer[y].clear(color);
        }
        else {
            auto& cache = _decodeLine(y);
            cache.setWriteFlag(true);
            int16_t endX = w + x;
            limitX(x, endX);
            while (x < endX) {
                cache.setPixel(x++, color);
            }
        }
    }
}

void GFXCanvasCompressed::drawInto(int16_t x, int16_t y, int16_t w, int16_t h, DrawLineCallback_t callback)
{
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    MicrosTimer timer;
    timer.start();
#endif
    // clipping
    int16_t y1, y2, x1, x2;
    int16_t yOfs, xOfs;
    if (y < 0) {
        y1 = 0;
        yOfs = y;
    }
    else {
        y1 = y;
        yOfs = 0;
    }
    y2 = y + h;
    if (y2 > _height) {
        y2 = _height;
    }
    if (x < 0) {
        x1 = 0;
        xOfs = x;
    }
    else {
        x1 = x;
        xOfs = 0;
    }
    x2 = x + w;
    if (x2 > _width) {
        x2 = _width;
    }
    w = x2 - x1;
    for(int16_t y = y1; y < y2; y++) {
        auto &cache = _decodeLine(y - yOfs);
        auto ptr = cache.getBuffer() - xOfs;

        callback(x, y, w, ptr);
        //gfx.drawRGBBitmap(x, y, ptr, w, 1);
    }

#if GFXCANVAS_MAX_CACHED_LINES < 2
    freeLineCache();
#endif
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.drawInto += timer.getTime();
#endif
}

void GFXCanvasCompressed::drawInto(DrawLineCallback_t callback)
{
    drawInto(0, 0, _width, _height, callback);
}

#if GFXCANVAS_MAX_CACHED_LINES < 2

void GFXCanvasCompressed::flushLineCache()
{
    if (_cache.hasWriteFlag()) {
        _encodeLine(_cache);
    }
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.cache_flush++;
#endif
}

void GFXCanvasCompressed::freeLineCache()
{
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.malloc++;
    stats.cache_drop++;
#endif
    flushLineCache();
    _cache.freeBuffer();
}

Cache& GFXCanvasCompressed::_getCache(int16_t y)
{
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (!((uint16_t)y < (uint16_t)_height)) {
        __debugbreak_and_panic();
    }
#endif
    if (_cache.isY(y)) {
        return _cache;
    }
    if (_cache.hasWriteFlag()) {
        _encodeLine(_cache);
    }
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.cache_max = _width * 2;
    stats.malloc++;
#endif
    if (!_cache.isValid()) {
        _cache.allocBuffer();
    }
    _cache.setY(y);
    return _cache;
}

#else

void GFXCanvasCompressed::flushLineCache()
{
    for (auto& cache : _caches) {
        if (cache.hasWriteFlag()) {
            _encodeLine(cache);
        }
    }
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.cache_flush++;
#endif
}

void GFXCanvasCompressed::freeLineCache()
{
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.malloc += _maxCachedLines * 2;
    stats.cache_drop++;
#endif
    flushLineCache();
    _caches.clear();
}

void GFXCanvasCompressed::setMaxCachedLines(uint8_t max)
{
    _maxCachedLines = max;
}


Cache &GFXCanvasCompressed::_getCache(int16_t y)
{
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (!((uint16_t)y < (uint16_t)_height)) {
        __debugbreak_and_panic();
    }
#endif
    // is the line cached?
    for(auto &cache: _caches) {
        if (cache.isY(y)) {
            return cache;
        }
    }
    uint8_t counter = 0;
    CacheForwardListIterator back;
    // no cache available, find invalid ones
    for (auto iterator = _caches.begin(); iterator != _caches.end(); ++iterator) {
        Cache &cache = *iterator;
        if (!cache.isValid()) {
            cache.setY(y);
            return cache;
        }
        counter++;
        back = iterator;
    }
    if (counter < _maxCachedLines) {
    // can we add more?
#if DEBUG_GFXCANVASCOMPRESSED_STATS
        stats.malloc += 2;
        stats.cache_max = std::max(stats.cache_max, (counter + 1) * _width * 2);
#endif
        if (counter == 0) {
            _caches.emplace_front(std::move(Cache(_width, y)));
            return *_caches.begin();
        }
        return *_caches.emplace_after(back, std::move(Cache(_width, y)));
    }
    else {
        // max cached lines reached, find one that has been written already and reuse it
        for(auto &cache: _caches) {
            if (!cache.hasWriteFlag()) {
                cache.setY(y);
                return cache;
            }
        }
    }
    // reuse front/oldest cache and move it to the back
    Cache &cache = _caches.front();
    if (cache.hasWriteFlag()) {
        _encodeLine(cache);
    }
    cache.setY(y);

#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.malloc += 1;
#endif
    std::swap(_caches.front(), *back);
    return *back;
}

#endif

void GFXCanvasCompressed::_RLEdecode(Buffer &buffer, uint16_t* output)
{
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    auto outputEndPtr = &output[_width];
    int count = 0;
#endif
    auto data = buffer.get();
    auto endPtr = &data[buffer.length()];
    while(data < endPtr - 2) {
        auto rle = *data++;
        uint16_t color = *data++;
        color |= (*data++ << 8);
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
        if (!(&output[rle] <= outputEndPtr)) {
            __debugbreak_and_panic();
        }
        count += rle;
#endif
        while(rle--) {
            *output++ = color;
        }
    }
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (count != _width) {
        __debugbreak_and_panic();
    }
#endif
}

void GFXCanvasCompressed::_RLEencode(uint16_t *data, Buffer &buffer)
{
    uint8_t rle = 0;
    auto endPtr = &data[_width];
    auto lastColor = *data;
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
   int count = 0;
#endif

    while(data < endPtr) {
        auto color = *data++;
        if (color == lastColor && rle != 0xff) {
            rle++;
        }
        else {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
            count += rle;
#endif
            buffer.write(rle);
            buffer.write(lastColor);
            buffer.write(lastColor >> 8);
            lastColor = color;
            rle = 1;
        }
    }
    if (data == endPtr) {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
        count += rle;
#endif
        buffer.write(rle);
        buffer.write(lastColor);
        buffer.write(lastColor >> 8);
    }
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (count != _width) {
        __debugbreak_and_panic();
    }
#endif
}

uint16_t GFXCanvasCompressed::getColor(uint16_t color, bool addIfNotExists)
{
    return color;
}

uint16_t *GFXCanvasCompressed::getPalette(uint8_t &count)
{
    count = 0;
    return nullptr;
}

void GFXCanvasCompressed::setPalette(uint16_t* palette, uint8_t count)
{
}

void GFXCanvasCompressed::_decodeLine(Cache &cache)
{
    if (_lineBuffer[cache.getY()].getLength() == 0) {

        auto color = _lineBuffer[cache.getY()].getFillColor();
        auto output = cache.getBuffer();

        auto count = _width;
        while(count--) {
            *output++ = color;
        }

    } else {

        _RLEdecode(_lineBuffer[cache.getY()].getBuffer(), cache.getBuffer());

    }
    cache.setReadFlag(true);
}

Cache &GFXCanvasCompressed::_decodeLine(int16_t y)
{
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (!((uint16_t)y < (uint16_t)_height)) {
        __debugbreak_and_panic();
    }
#endif
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    MicrosTimer timer;
    timer.start();
#endif

    auto &cache = _getCache(y);
    if (cache.hasReadFlag()) {
#if DEBUG_GFXCANVASCOMPRESSED_STATS
        stats.cache_read++;
#endif
        return cache;
    }
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (cache.hasWriteFlag()) {
        __debugbreak_and_panic(); // cache has write flag but no read flag
    }
#endif

    yield();

    _decodeLine(cache);

#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.decode_time += timer.getTime() / 1000.0;
    stats.decode_count++;
#endif
    return cache;
}

void GFXCanvasCompressed::_encodeLine(Cache &cache)
{
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (!((uint16_t)cache.getY() < (uint16_t)_height)) {
        __debugbreak_and_panic();
    }
#endif
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    MicrosTimer timer;
    timer.start();
#endif

    auto &buffer = _lineBuffer[cache.getY()].getBuffer();

#if DEBUG_GFXCANVASCOMPRESSED_STATS
    auto bufferSize = buffer.size();
#endif

    buffer.remove(0, buffer.length());
    _RLEencode(cache.getBuffer(), buffer);

#if 0
    uint16_t *temp2 = (uint16_t *)calloc(_width*2, 2);
    memcpy(temp2, cache.getBuffer(), _width * 2);
    uint16_t *temp = (uint16_t *)calloc(_width*2, 2);
    _RLEdecode(buffer, temp);
    for (int i = 0; i < _width; i++) {
        if (temp[i] != temp2[i]) {
            __debugbreak();
        }
    }
    free(temp2);
    free(temp);
#endif

#if DEBUG_GFXCANVASCOMPRESSED_STATS
    if (buffer.size() > bufferSize) {
        stats.malloc += (buffer.size() - bufferSize) / 16;
    }
#endif
    if (buffer.size() - buffer.length() > 32) { // shrink buffer if there is more than 32 byte available
#if DEBUG_GFXCANVASCOMPRESSED_STATS
        stats.malloc++;
#endif
        buffer.shrink(buffer.length());
    }
    cache.setWriteFlag(false);

#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.encode_time += timer.getTime() / 1000.0;
    stats.encode_count++;
#endif
}


String GFXCanvasCompressed::getDetails() const
{
    PrintString str;

    size_t size = 0;
    auto count = _height;
    while(count--) {
        size += _lineBuffer[count].getLength();
    }
    uint32_t rawSize = _width * _height * sizeof(uint16_t);
    int empty = 0;
    for (uint16_t y = 0; y < _height; y++) {
        if (_lineBuffer[y].getLength() == 0) {
            empty++;
        }
    }
    str.printf_P(PSTR("%ux%ux16 memory: %u/%u (%.2f%%) empty rows: %d\n"), _width, _height, size, rawSize, 100.0 - (size * 100.0 / (float)rawSize), empty);
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    stats.dump(str);
#endif
    return str;
}

GFXCanvasBitmapStream GFXCanvasCompressed::getBitmap()
{
    return GFXCanvasBitmapStream(*this);
}

GFXCanvasBitmapStream GFXCanvasCompressed::getBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    return GFXCanvasBitmapStream(*this, x, y, w, h);
}

GFXCanvasRLEStream GFXCanvasCompressed::getRLEStream()
{
    return GFXCanvasRLEStream(*this);
}

GFXCanvasRLEStream GFXCanvasCompressed::getRLEStream(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    return GFXCanvasRLEStream(*this, x, y, w, h);
}

Cache &GFXCanvasCompressed::getLine(int16_t y)
{
    return _decodeLine(y);
}
