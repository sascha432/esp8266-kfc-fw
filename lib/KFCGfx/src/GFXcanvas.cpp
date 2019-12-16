/**
* Author: sascha_lammers@gmx.de
*/

#if HAVE_GFX_LIB

#include "GFXcanvas.h"

#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace GFXCanvas;

// Stats

#if DEBUG_GFXCANVASCOMPRESSED_STATS

void Stats::dump(Print &output) const {
    output.printf_P(PSTR("de/encode %u/%.2fms, %u/%.2fms memop %u cache read %u flush %u drop %u max %ub drawInto %.3f ms\n"),
        decode_count, decode_time, encode_count, encode_time,
        malloc,
        cache_read, cache_flush, cache_drop, cache_max, drawInto / 1000.0
    );
}

#endif

// Cache

Cache::Cache() : _buffer(nullptr)
{
    _width = 0;
}

Cache::Cache(Cache &&cache)
{
    *this = std::move(cache);
}

Cache::Cache(uint16_t width, int16_t y) : _buffer(nullptr), _width(width)
{
    allocBuffer();
    _y = y;
    _read = 0;
    _write = 0;
}

GFXCanvas::Cache::Cache(uint16_t width) : _buffer(nullptr), _width(width)
{
    _y = Cache::INVALID;
    _read = 0;
    _write = 0;
}

Cache::~Cache()
{
    freeBuffer();
}

Cache &Cache::operator =(Cache &&cache)  {
    _buffer = cache._buffer;
    _read = cache._read;
    _write = cache._write;
    _y = cache._y;
    _width = cache._width;
    cache._buffer = nullptr;
    cache.setY(INVALID);
    return *this;
}

void GFXCanvas::Cache::allocBuffer()
{
    if (!_buffer) {
        size_t size = _width * sizeof(*_buffer);
        _buffer = (uint16_t*)malloc(size);
        if (!_buffer) {
            debug_printf("Cache(): malloc %u failed\n", size);
            __debugbreak_and_panic();
        }
    }
}

void GFXCanvas::Cache::freeBuffer()
{
    setY(Cache::INVALID);
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
}

bool Cache::isY(int16_t y) const {
    return _y == y;
}

int16_t Cache::getY() const {
    return _y;
}

bool Cache::isValid() const {
    return _y != INVALID;
}

void Cache::setY(int16_t y) {
    _read = 0;
    _write = 0;
    _y = y;
}

bool Cache::hasWriteFlag() const {
    return _write;
}

void Cache::setWriteFlag(bool value) {
    _write = value;
}

bool Cache::hasReadFlag() const {
    return _read;
}

void Cache::setReadFlag(bool value) {
    _read = value;
}


uint16_t *Cache::getBuffer() const {
    return _buffer;
}

// LineBuffer

LineBuffer::LineBuffer() : _fillColor(0) {
}

void LineBuffer::clear(uint16_t fillColor) {
    _fillColor = fillColor;
    _buffer.clear();
}

uint16_t LineBuffer::getLength() const {
    return (uint16_t)_buffer.length();
}

Buffer &LineBuffer::getBuffer() {
    return _buffer;
}

void GFXCanvas::LineBuffer::clone(LineBuffer& source)
{
    _buffer.write(source._buffer.get(), source._buffer.length());
    _fillColor = source._fillColor;
}

// functions

void GFXCanvas::convertToRGB(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b)
{
    r = ((color >> 11) * 527 + 23) >> 6;
    g = (((color >> 5) & 0x3f) * 259 + 33) >> 6;
    b = ((color & 0x1f) * 527 + 23) >> 6;
}

uint32_t GFXCanvas::convertToRGB(uint16_t color)
{
    uint8_t r, g, b;
    convertToRGB(color, r, g, b);
    return (r << 16) | (g << 8) | b;
}

uint16_t GFXCanvas::convertRGBtoRGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((b >> 3) & 0x1f) | (((g >> 2) & 0x3f) << 5) | (((r >> 3) & 0x1f) << 11);
}

uint16_t GFXCanvas::convertRGBtoRGB565(uint32_t rgb)
{
    return convertRGBtoRGB565(rgb, rgb >> 8, rgb >> 16);
}

#endif
