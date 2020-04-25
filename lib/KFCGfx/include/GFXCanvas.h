/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <assert.h>
#include <MicrosTimer.h>

#include <push_pack.h>

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

    using coord_x_t = uint16_t;
    using coord_y_t = uint16_t;
    using scoord_x_t = int16_t;
    using scoord_y_t = int16_t;
    using color_t = uint16_t;

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

    class ByteBuffer {
    public:
        using coord_x_t = uint16_t;

        ByteBuffer() : _buffer(nullptr), _length(0), _size(0) {
        }
        ~ByteBuffer() {
            if (_buffer) {
                free(_buffer);
            }
        }

        uint8_t *get() const {
            return _buffer;
        }

        size_t length() const {
            return _length;
        }

        size_t size() const {
            return _size;
        }

        void clear() {
            if (_buffer) {
                free(_buffer);
                _buffer = nullptr;
            }
            _size = 0;
            _length = 0;
        }

        size_t write(const uint8_t *data, size_t len) {
            if (!reserve(_length + len)) {
                return 0;
            }
            memmove(_buffer + _length, data, len);
            _length += len;
            return len;
        }

        inline size_t write(int data) {
            return write((uint8_t)data);
        }

        size_t write(uint8_t data) {
            return write(&data, sizeof(data));
        }

        bool shrink(size_t newSize) {
            if (newSize == 0) {
                newSize = _length;
            }
            if (_changeBuffer(newSize)) {
                return true;
            }
            clear();
            return false;
        }

        bool reserve(size_t size) {
            if (size > _size) {
                if (!_changeBuffer(size)) {
                    return false;
                }
            }
            return true;
        }

        bool _changeBuffer(size_t newSize) {
            if (_alignSize(newSize) != _size) {
                _size = _alignSize(newSize);
                if (_size == 0) {
                    if (_buffer) {
                        free(_buffer);
                        _buffer = nullptr;
                    }
                }
                else {
                    if (_buffer == nullptr) {
                        _buffer = (uint8_t *)malloc(_size);
                    }
                    else {
                        _buffer = (uint8_t *)realloc(_buffer, _size);
                    }
                    if (_buffer == nullptr) {
                        _size = 0;
                        _length = 0;
                        return false;
                    }
                }
            }
            if (_length > _size) {
                _length = _size;
            }
            return true;
        }

        void remove(size_t index, size_t count) {
            if (index >= _length) {
                return;
            }
            if (count <= 0) {
                return;
            }
            if (count > _length - index) {
                count = _length - index;
            }
            _length = _length - count;
            if (_length - index) {
                memmove(_buffer + index, _buffer + index + count, _length - index);
            }
        }

        inline uint16_t _alignSize(size_t size) const {
            return (size + 7) & ~7;
        }

    private:
        uint8_t *_buffer;
        size_t _length;
        size_t _size;
    };


    class Cache {
    public:
        static const scoord_y_t INVALID = -1;

        explicit Cache();
        Cache(const Cache& cache) = delete;
        //    Cache(const Cache &cache) {
        //    __debugbreak_and_panic();
        //}
        Cache(Cache &&cache);
        Cache(coord_x_t width, scoord_y_t y);
        Cache(coord_x_t width);
        ~Cache();

        Cache &operator =(const Cache &cache) = delete;
        Cache &operator =(Cache &&cache);

        void allocBuffer();
        void freeBuffer();

        bool isY(scoord_y_t y) const;
        scoord_y_t getY() const;
        void setY(scoord_y_t y);               // if the y position is changed, flags are cleared. in case the write flag is set, the cache must be written before
        bool isValid() const;

        bool hasWriteFlag() const;          // indicates that the cache has not been written
        void setWriteFlag(bool value);

        bool hasReadFlag() const;           // indicates that the cache contains a copy
        void setReadFlag(bool value);

        color_t *getBuffer() const;

        inline void setPixel(scoord_x_t x, color_t color) {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
            if (!(x >= 0 && x < _width)) {
                __debugbreak_and_panic();
            }
#endif
            _buffer[x] = color;
        }


    private:
        color_t *_buffer;
        scoord_y_t _y;
        coord_x_t _width;
        uint8_t _read: 1;
        uint8_t _write: 1;
    };

    class LineBuffer {
    public:
        LineBuffer();

        void clear(color_t fillColor);
        coord_x_t getLength() const;
        ByteBuffer &getBuffer();
        void setFillColor(color_t fillColor) {
            _fillColor = fillColor;
        }
        color_t getFillColor() const {
            return _fillColor;
        }

        void clone(LineBuffer& source);

    private:
        ByteBuffer _buffer;
        color_t _fillColor;
    };

    void convertToRGB(color_t color, uint8_t& r, uint8_t& g, uint8_t& b);
    uint32_t convertToRGB(color_t color);
    color_t convertRGBtoRGB565(uint8_t r, uint8_t g, uint8_t b);
    color_t convertRGBtoRGB565(uint32_t rgb);

};

#include <pop_pack.h>
