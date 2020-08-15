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
        LineBuffer(ByteBuffer &buffer, ColorType fillColor);
        ~LineBuffer();

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

}
