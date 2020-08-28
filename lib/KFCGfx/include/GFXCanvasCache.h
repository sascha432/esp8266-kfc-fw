/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

class GFXCanvasCompressed;
class GFXCanvasRLEStream;

namespace GFXCanvas {

    class Cache {
    public:
        static constexpr sYType kYInvalid = -1;

        Cache() = delete;
        Cache(const Cache& cache) = delete;

        Cache(Cache &&cache) noexcept;
        Cache(uWidthType width, sYType y);
        Cache(uWidthType width, nullptr_t);
        ~Cache();

        Cache &operator =(const Cache &cache) = delete;
        Cache &operator =(Cache &&cache) noexcept;

        // allocate buffer if nullptr
        void allocate();
        // release memory
        void release();

        inline bool isY(sYType y) const
        {
            return _y == y;
        }

        inline sYType getY() const
        {
            return _y;
        }

        inline void setY(sYType y)
        {
            _flags = 0;
            _y = y;
        }

        // invalidate cache
        inline void clear()
        {
            invalidate();
        }

        inline void invalidate()
        {
            _flags = 0;
            _y = kYInvalid;
        }
        // if the y position is changed, flags are cleared. in case the write flag is set, the cache must be written before
        inline bool isValid() const {
            return _y != kYInvalid;
        }

        inline bool hasWriteFlag() const {
            return _write;
        }

        // indicates that the cache has not been written
        inline void setWriteFlag(bool value) {
            _write = value;
        }

        inline bool hasReadFlag() const {
            return _read;
        }

        // indicates that the cache contains a copy
        inline void setReadFlag(bool value) {
            _read = value;
        }

        inline ColorType *getBuffer() const
        {
            __LDBG_check_alloc_no_null(_buffer);
            __LDBG_assert(_y != kYInvalid);
            return _buffer;
        }

        void fill(ColorType color);
        void fill(ColorType color, uWidthType startX, uWidthType endX);
        // deprecated
        void setPixel(sXType x, ColorType color);

        inline ColorType *begin() const {
            return _buffer;
        }

        inline ColorType *end() const {
            return &_buffer[_width];
        }

    protected:
        friend GFXCanvasRLEStream;

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

    inline void Cache::fill(ColorType color)
    {
        __DBG_BOUNDS_RETURN(__DBG_check_alloc_aligned(_buffer));
        std::fill(begin(), end(), color);
    }

    inline void Cache::setPixel(sXType x, ColorType color)
    {
        __DBG_BOUNDS_RETURN(
            __DBG_BOUNDS_sx(x, _width) ||
            __DBG_check_alloc_aligned(_buffer)
        );
        _buffer[x] = color;
    }



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

    inline void SingleLineCache::invalidateRange(sYType startY, sYType endY)
    {
        invalidate();
    }

    inline void SingleLineCache::free(GFXCanvasCompressed &canvas)
    {
        flush(canvas);
        release();
    }

    inline void SingleLineCache::resize(GFXCanvasCompressed &canvas, size_t numLines)
    {
        __LDBG_printf("lines=%u", numLines);
        flush(canvas);
    }

    inline size_t SingleLineCache::length() const
    {
        return 1;
    }
#if GFXCANVAS_MAX_CACHED_LINES < 2
    using LinesCache = SingleLineCache;
#else
    #error missing
#endif

}

