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

using namespace GFXCanvas;

#include "GFXCanvasLineBuffer.h"

LineBuffer::LineBuffer() : _buffer(), _fillColor(0)
{
}

LineBuffer::LineBuffer(ByteBuffer &buffer, ColorType fillColor) : _buffer(buffer), _fillColor(fillColor)
{
}

LineBuffer::~LineBuffer()
{
}

LineBuffer &LineBuffer::operator=(const LineBuffer &source)
{
    _buffer = source._buffer;
    _fillColor = source._fillColor;
    return *this;
}

void LineBuffer::clear(ColorType fillColor)
{
    _fillColor = fillColor;
    _buffer.clear();
}

uWidthType LineBuffer::length() const
{
    return (uWidthType)_buffer.length();
}

ByteBuffer &LineBuffer::getBuffer()
{
    return _buffer;
}

const ByteBuffer &LineBuffer::getBuffer() const
{
    return _buffer;
}

void LineBuffer::setFillColor(ColorType fillColor)
{
    _fillColor = fillColor;
}

ColorType LineBuffer::getFillColor() const
{
    return _fillColor;
}

#include <pop_optimize.h>
