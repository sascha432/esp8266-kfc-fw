/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasLineBuffer.h"

class GFXCanvasCompressed;
class GFXCanvasRLEStream;

namespace GFXCanvas {

    class Lines {
    public:
        Lines(Lines &&) = delete;

        Lines();
        Lines(uHeightType height);

        // height must match
        Lines(const Lines &_lines);

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


        inline LineBuffer *begin() const {
            return _lines;
        }

        inline LineBuffer *end() const {
            return &_lines[_height];
        }

    private:
        friend GFXCanvasCompressed;
        friend GFXCanvasRLEStream;

        uHeightType _height;
        LineBuffer *_lines;
    };

    inline void Lines::clear(ColorType color)
    {
        fill(color, 0, _height);
    }

    inline void Lines::fill(ColorType color, uYType y)
    {
        getLine(y).clear(color);
    }

    inline uHeightType Lines::height() const
    {
        return _height;
    }

    inline ColorType Lines::getFillColor(sYType y) const
    {
        return getLine(y).getFillColor();
    }

    inline ByteBuffer &Lines::getBuffer(sYType y)
    {
        return getLine(y).getBuffer();
    }

    inline const ByteBuffer &Lines::getBuffer(sYType y) const
    {
        return getLine(y).getBuffer();
    }

    inline LineBuffer &Lines::getLine(sYType y)
    {
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _height), y = 0);
        return _lines[y];
    }

    inline const LineBuffer &Lines::getLine(sYType y) const
    {
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _height), y = 0);
        return _lines[y];
    }

    inline uWidthType Lines::length(sYType y) const
    {
        return getLine(y).length();
    }

}
