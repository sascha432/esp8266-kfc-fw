/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <assert.h>
#include <Buffer.h>
#include <MicrosTimer.h>

#ifndef DEBUG_GFXCANVAS
#define DEBUG_GFXCANVAS                             1
#endif

#if _MSC_VER
#define DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK      1
#define DEBUG_GFXCANVASCOMPRESSED_STATS             1
#define GFXCANVAS_MAX_CACHED_LINES                  255
#endif

#ifndef GFXCANVAS_MAX_CACHED_LINES
#define GFXCANVAS_MAX_CACHED_LINES                  1
#endif

// enable for debugging only
#ifndef DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
#define DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK      1
#endif

#ifndef DEBUG_GFXCANVASCOMPRESSED_STATS
#define DEBUG_GFXCANVASCOMPRESSED_STATS             1
#endif

namespace GFXCanvas {

#if DEBUG_GFXCANVASCOMPRESSED_STATS
    class Stats {
    public:
        Stats() {
            memset(this, 0, sizeof(*this));
        }
        void dump(Print &output) const;

        int decode_count;
        int encode_count;
        double decode_time;
        double encode_time;
        int decode_invalid;
        int encode_invalid;
        int cache_read;
        int cache_flush;
        int cache_drop;
        int cache_max;
        uint32_t drawInto;
        int malloc;
    };
#endif

    class Cache {
    public:
        static const int16_t INVALID = -1;

        explicit Cache();
        Cache(const Cache& cache) = delete;
        //    Cache(const Cache &cache) {
        //    __debugbreak_and_panic();
        //}
        Cache(Cache &&cache);
        Cache(uint16_t width, int16_t y);
        Cache(uint16_t width);
        ~Cache();

        Cache &operator =(const Cache &cache) = delete;
        Cache &operator =(Cache &&cache);

        void allocBuffer();
        void freeBuffer();

        bool isY(int16_t y) const;
        int16_t getY() const;
        void setY(int16_t y);               // if the y position is changed, flags are cleared. in case the write flag is set, the cache must be written before
        bool isValid() const;

        bool hasWriteFlag() const;          // indicates that the cache has not been written
        void setWriteFlag(bool value);

        bool hasReadFlag() const;           // indicates that the cache contains a copy
        void setReadFlag(bool value);

        uint16_t *getBuffer() const;

        inline void setPixel(int16_t x, uint16_t color) {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
            if (!(x >= 0 && x < _width)) {
                __debugbreak_and_panic();
            }
#endif
            _buffer[x] = color;
        }


    private:
        uint16_t *_buffer;
        int16_t _y;
        uint16_t _width;
        uint8_t _read: 1;
        uint8_t _write: 1;
    };

    class LineBuffer {
    public:
        LineBuffer();

        void clear(uint16_t fillColor);
        uint16_t getLength() const;
        Buffer &getBuffer();
        void setFillColor(uint16_t fillColor) {
            _fillColor = fillColor;
        }
        uint16_t getFillColor() const {
            return _fillColor;
        }

        void clone(LineBuffer& source);

    private:
        Buffer _buffer;
        uint16_t _fillColor;
    };

    void convertToRGB(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b);
    uint32_t convertToRGB(uint16_t color);
    uint16_t convertRGBtoRGB565(uint8_t r, uint8_t g, uint8_t b);
    uint16_t convertRGBtoRGB565(uint32_t rgb);

};
