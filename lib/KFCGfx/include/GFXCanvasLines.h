/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasLineBuffer.h"

class GFXCanvasCompressed;

namespace GFXCanvas {

    class Lines {
    public:

        Lines(const Lines &lines) = delete;
        Lines(Lines &&) = delete;

        Lines &operator=(const Lines &lines);

        Lines();
        Lines(uHeightType height, const LineBuffer *_lines);
        Lines(uHeightType height);
        ~Lines();

        uHeightType height() const;

        void clear(ColorType color);
        void fill(ColorType, uYType start, uYType end);
        void fill(ColorType, uYType y);

        ColorType getFillColor(sYType y) const;
        uWidthType length(sYType y) const;

        ByteBuffer &getBuffer(sYType y);
        const ByteBuffer &getBuffer(sYType y) const;

        LineBuffer &getLine(sYType y);
        const LineBuffer &getLine(sYType y) const;

    private:
        friend GFXCanvasCompressed;

        uHeightType _height;
        LineBuffer *_lines;
    };

}
