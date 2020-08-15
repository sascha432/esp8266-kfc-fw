/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

class GFXCanvasCompressed;

namespace GFXCanvas {

    class Cache {
    public:
        static constexpr sYType kYInvalid = -1;

        Cache() = delete;
        Cache(const Cache& cache) = delete;

        Cache(Cache &&cache);
        Cache(uWidthType width, sYType y);
        Cache(uWidthType width, nullptr_t);
        ~Cache();

        Cache &operator =(const Cache &cache) = delete;
        Cache &operator =(Cache &&cache);

        // allocate buffer if nullptr
        void allocate();
        // release memory
        void release();

        bool isY(sYType y) const
        {
            return _y == y;
        }

        sYType getY() const
        {
            return _y;
        }

        void setY(sYType y)
        {
            _flags = 0;
            _y = y;
        }

        // invalidate cache
        void clear() {
            invalidate();
        }

        void invalidate()
        {
            _flags = 0;
            _y = kYInvalid;
        }

        // use after std::move() or if allocating the buffer failed
        void __reset()
        {
            _buffer = nullptr;
            _y = kYInvalid;
            _width = 0;
            _flags = 0;
        }

        // if the y position is changed, flags are cleared. in case the write flag is set, the cache must be written before
        bool isValid() const {
            return _y != kYInvalid;
        }

        bool hasWriteFlag() const {
            return _write;
        }

        // indicates that the cache has not been written
        void setWriteFlag(bool value) {
            _write = value;
        }

        bool hasReadFlag() const {
            return _read;
        }

        // indicates that the cache contains a copy
        void setReadFlag(bool value) {
            _read = value;
        }

        ColorType *getBuffer() const
        {
            __LDBG_check_alloc_no_null(_buffer);
            __LDBG_assert(_y != kYInvalid);
            return _buffer;
        }

        void fill(ColorType color);
        void fill(ColorType color, uWidthType startX, uWidthType endX);
        // deprecated
        void setPixel(sXType x, ColorType color);

    protected:
        ColorType *_buffer;
        sYType _y;
        uWidthType _width;
        union {
            struct {
                uint8_t _read: 1;
                uint8_t _write: 1;
            };
            uint8_t _flags;
        };
    };

    class SingleLineCache : public Cache
    {
    public:
        SingleLineCache(uWidthType width, uHeightType height, size_t numLines);

        Cache &get(GFXCanvasCompressed &canvas, sYType y);
        void invalidateRange(sYType startY, sYType endY);

        // flush cache and write to line buffer
        void flush(GFXCanvasCompressed &canvas);
        // flush cache and free memory
        void free(GFXCanvasCompressed &canvas);

        void resize(GFXCanvasCompressed &canvas, size_t numLines);
        size_t length() const;
    };


#if GFXCANVAS_MAX_CACHED_LINES < 2
    using LinesCache = SingleLineCache;
#else
    #error missing
#endif

}

