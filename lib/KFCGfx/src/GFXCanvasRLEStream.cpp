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

size_t GFXCanvasRLEStream::readBytes(char *buffer, size_t length)
{
    size_t sent = 0;
    while (available() && length) {
        if (_buffer.length() == 0 && _read(true) == -1) { // fill the buffer by calling peek()
            break;
        }
        size_t canSend = _buffer.length();
        if (canSend) { // we have some buffered data
            if (canSend > length) {
                canSend = length;
            }
            memcpy(buffer, _buffer.begin(), canSend);
            _buffer.remove(0, canSend);
            length -= canSend;
            sent += canSend;
        }
    }
    return sent;
}

int GFXCanvasRLEStream::_read(bool peek)
{
    if (available()) {
        if (_buffer.length()) {
            return _sendBufferedByte(peek);
        }
        else {
            int32_t imageSize = _header.width * _header.height;
            uint16_t x = (_position % _header.width) + _header.x;
            uint16_t y = (_position / _header.width) + _header.y;

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
                    _writeColor((uint8_t)rle, _lastColor);
                    _lastColor = color;
                    rle = 0;
                    if (_position == imageSize) {
                        break;
                    }
                    if (_buffer.length() > 32) {
                        return _sendBufferedByte(peek);
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

            _writeColor((uint8_t)rle, color);
            _position = DONE;
            return _sendBufferedByte(peek);
        }
    }
    return -1;
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

uint8_t GFXCanvasRLEStream::_sendBufferedByte(bool peek)
{
    auto byte = *_buffer.get();
    if (!peek) {
        _buffer.remove(0, 1);
    }
    return byte;
}

#include <pop_optimize.h>
