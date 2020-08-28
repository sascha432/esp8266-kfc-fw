/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasByteBuffer.h"

namespace GFXCanvas {

    class LineBuffer {
    public:

        LineBuffer(const LineBuffer &) = delete;
        LineBuffer(LineBuffer &&) = delete;
        LineBuffer &operator=(LineBuffer &&source) = delete;

        LineBuffer();

        LineBuffer &operator=(const LineBuffer &source);

        void clear(ColorType fillColor);
        uWidthType length() const;
        ByteBuffer &getBuffer();
        const ByteBuffer &getBuffer() const;
        void setFillColor(ColorType fillColor);
        ColorType getFillColor() const;

    private:
        ByteBuffer _buffer;
        ColorType _fillColor;
    };

    inline LineBuffer::LineBuffer() : _fillColor(0)
    {
    }

    inline LineBuffer &LineBuffer::operator=(const LineBuffer &source)
    {
        _buffer = source._buffer;
        _fillColor = source._fillColor;
        return *this;
    }

    inline void LineBuffer::clear(ColorType fillColor)
    {
        _fillColor = fillColor;
        _buffer.clear();
    }

    inline uWidthType LineBuffer::length() const
    {
        return (uWidthType)_buffer.length();
    }

    inline ByteBuffer &LineBuffer::getBuffer()
    {
        return _buffer;
    }

    inline const ByteBuffer &LineBuffer::getBuffer() const
    {
        return _buffer;
    }

    inline void LineBuffer::setFillColor(ColorType fillColor)
    {
        _fillColor = fillColor;
    }

    inline ColorType LineBuffer::getFillColor() const
    {
        return _fillColor;
    }

}
