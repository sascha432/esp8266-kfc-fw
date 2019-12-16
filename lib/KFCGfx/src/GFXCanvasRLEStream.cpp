/**
 * Author: sascha_lammers@gmx.de
 */

#if HAVE_GFX_LIB

#include "GFXCanvasRLEStream.h"
#include "GFXCanvasCompressed.h"

GFXCanvasRLEStream::GFXCanvasRLEStream(GFXCanvasCompressed& canvas) : GFXCanvasRLEStream(canvas, 0, 0, _canvas.width(), canvas.height())
{
}

GFXCanvasRLEStream::GFXCanvasRLEStream(GFXCanvasCompressed& canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h) : _canvas(canvas), _cache(w, Cache::INVALID), _position(0), _lastColor(-1)
{
    _header.x = x;
    _header.y = y;
    _header.width = w;
    _header.height = h;
    _header.paletteCount = 0;

    uint8_t count;
    if (canvas.getPalette(count) != nullptr) {
        _canvas.flushLineCache(); // flush cache to get the palette filled
        auto palette = canvas.getPalette(count);
        _header.paletteCount = count;
        _buffer.write((uint8_t*)&_header, sizeof(_header));
        _buffer.write((uint8_t*)palette, count * sizeof(*palette));
    }
    else {
        _buffer.write((uint8_t*)&_header, sizeof(_header));
    }
}

int GFXCanvasRLEStream::available()
{
    if (_buffer.length()) {
        return _buffer.length();
    }
    return _position != DONE ? 1 : 0;
}

int GFXCanvasRLEStream::read()
{
    if (available()) {

        if (_buffer.length()) {
            return _sendBufferedByte();
        }
        else {
            int32_t imageSize = _header.width * _header.height;
            uint16_t x = (_position % _header.width) + _header.x;
            uint16_t y = (_position / _header.width) + _header.y;

            if (!_cache.isY(y)) {
                _cache.setY(y);
                _canvas._decodeLine(_cache);
            }

            auto color = _canvas.getColor(_cache.getBuffer()[x], false);
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
                        return _sendBufferedByte();
                    }
                }

                x = (_position % _header.width) + _header.x;
                y = (_position / _header.width) + _header.y;

                if (!_cache.isY(y)) {
                    _cache.setY(y);
                    _canvas._decodeLine(_cache);
                }

                color = _canvas.getColor(_cache.getBuffer()[x], false);
            }

            _writeColor((uint8_t)rle, color);
            _position = DONE;
            return _sendBufferedByte();
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
        _buffer.write(color);
        _buffer.write(color >> 8);
    }
}

uint8_t GFXCanvasRLEStream::_sendBufferedByte()
{
    auto byte = *_buffer.get();
    _buffer.remove(0, 1);
    return byte;
}

#endif
