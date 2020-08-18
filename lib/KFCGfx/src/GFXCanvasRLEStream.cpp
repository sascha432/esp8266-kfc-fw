/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#include <push_optimize.h>
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

#include "GFXCanvasRLEStream.h"
#include "GFXCanvasCompressed.h"
#include "GFXCanvas.h"

using namespace GFXCanvas;


GFXCanvasRLEStream::GFXCanvasRLEStream(GFXCanvasCompressed& canvas) : GFXCanvasRLEStream(canvas, 0, 0, canvas.width(), canvas.height())
{
}

GFXCanvasRLEStream::GFXCanvasRLEStream(GFXCanvasCompressed& canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h) : _canvas(canvas), _cache(w, Cache::kYInvalid), _position(0), _lastColor(-1)
{
    _header.x = x;
    _header.y = y;
    _header.width = w;
    _header.height = h;
    _header.paletteCount = 0;

    auto palette = _canvas.getPalette();
    if (palette) {
        _canvas.flushCache();
        _header.paletteCount = palette->length();
        _buffer.writeObject(_header);
        _buffer.write(palette->getBytes(), palette->getBytesLength());
    }
    else {
        _buffer.writeObject(_header);
    }
    // __DBG_printf("header=%u", _buffer.length());
}

int GFXCanvasRLEStream::available()
{
    if (_buffer.length()) {
        return _buffer.length();
    }
    return _position != DONE ? 1 : 0;
}

size_t GFXCanvasRLEStream::readBytes(uint8_t *buffer, size_t length)
{
    // first, empty the buffer
    auto begin = buffer;
    size_t bufferLen = _buffer.length();
    while(bufferLen) {
        if (bufferLen > length) {
            bufferLen = length;
        }
        memcpy(begin, _buffer.begin(), bufferLen);
        _buffer.remove(0, bufferLen);
        begin += bufferLen;
        length -= bufferLen;
        if (length == 0) {
            return begin - buffer;
        }
        bufferLen = _buffer.length();
    }
    if (_position == DONE) {
        return 0;
    }

    // now we can read directly into the provided buffer
    int read = _fillBuffer(begin, length);
    if (read < 0) { // error, stop reading
        _position = DONE;
        _buffer.clear();
        return 0;
    }
    return read + (begin - buffer);
}

int GFXCanvasRLEStream::_fillBuffer(uint8_t *buffer, size_t length)
{
    int32_t imageSize = _header.width * _header.height;
    uint16_t x = (_position % _header.width) + _header.x;
    uint16_t y = (_position / _header.width) + _header.y;

    auto begin = buffer;
    uint8_t *end;
    end = begin + length - 6; // we need 2x3 bytes reserved for _writeColor

    // __DBG_printf("x=%u y=%u pos=%u image_size=%u", x, y, _position, imageSize);

    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _canvas.height()), return -1);
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sx(x, _canvas.width()), return -1);
    if (!_cache.isY(y)) {
        _cache.setY(y);
        _canvas._decodeLine(_cache);
    }
    auto color = _canvas.getPaletteColor(_cache.getBuffer()[x]);
    int16_t rle;
    if (_position == 0) {
        _lastColor = color;
        rle = -1;
    }
    else {
        rle = 0;
    }
    while (_position++ < imageSize) {
        if (color == _lastColor && rle != 0xff) {
            rle++;
            if (_position == imageSize) {
                break;
            }
        }
        else {
            begin = _writeColor(begin, (uint8_t)rle, _lastColor);
            _lastColor = color;
            rle = 0;
            if (_position == imageSize) {
                break;
            }
            if (begin >= end) {
                return begin - buffer;
            }
        }

        x = (_position % _header.width) + _header.x;
        y = (_position / _header.width) + _header.y;

        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _canvas.height()), return -1);
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sx(x, _canvas.width()), return -1);
        if (!_cache.isY(y)) {
            _cache.setY(y);
            _canvas._decodeLine(_cache);
        }
        color = _canvas.getPaletteColor(_cache.getBuffer()[x]);
    }

    begin = _writeColor(begin, (uint8_t)rle, color);
    _position = DONE;
    return begin - buffer;

}
int GFXCanvasRLEStream::read()
{
    if (_buffer.length()) {
        return _buffer.read();
    }
    return _read(false);
}

int GFXCanvasRLEStream::peek()
{
    if (_buffer.length()) {
        return _buffer[0];
    }
    return _read(true);
}

int GFXCanvasRLEStream::_read(bool peek)
{
    if (_buffer.length() == 0) {
        if (_position == DONE) {
            return -1;
        }
        _buffer.reserve(128);
        int read;
        if ((read = _fillBuffer(_buffer.begin(), _buffer.size())) <= 0) {
            // error, stop reading
            _buffer.clear();
            _position = DONE;
            return -1;
        }
        _buffer.setLength(read);
    }
    return peek ? _buffer.peek() : _buffer.read();
}

void GFXCanvasRLEStream::_writeColor(uint8_t rle, uint16_t color)
{
    if (_header.paletteCount) {
        if (rle >= 0xf) {
            _buffer.write((color << 4) | 0xf);
            _buffer.write(rle);
        }
        else {
            _buffer.write((color << 4) | rle);
        }
    }
    else {
        _buffer.write(rle);
        _buffer.write((uint8_t)color);
        _buffer.write((uint8_t)(color >> 8));
    }
}

uint8_t *GFXCanvasRLEStream::_writeColor(uint8_t *buffer, uint8_t rle, uint16_t color)
{
    if (_header.paletteCount) {
        if (rle >= 0xf) {
            *buffer++ = (color << 4) | 0xf;
            *buffer++ = rle;
        }
        else {
            *buffer++ = (color << 4) | rle;
        }
    }
    else {
        *buffer++ = rle;
        *buffer++ = color;
        *buffer++ = color >> 8;
    }
    return buffer;
}

#include <pop_optimize.h>
