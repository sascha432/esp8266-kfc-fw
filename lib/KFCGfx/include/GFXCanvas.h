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
#define DEBUG_GFXCANVASCOMPRESSED_STATS_DETAILS     1
#define GFXCANVAS_MAX_CACHED_LINES                  16
#endif

// max. number of cached lines
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

#ifndef DEBUG_GFXCANVASCOMPRESSED_STATS_DETAILS
#define DEBUG_GFXCANVASCOMPRESSED_STATS_DETAILS     0
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
        Stats() = default;
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
        int modified_skipped;
    };
#endif

    class ByteBuffer {
    public:
        using size_t = uint16_t;

        ByteBuffer() : _buffer(nullptr), _length(0), _size(0) {
        }
        ~ByteBuffer() {
            if (_buffer) {
                free(_buffer);
            }
        }

        inline __attribute__((always_inline)) uint8_t *get() const {
            return _buffer;
        }

        inline __attribute__((always_inline)) size_t length() const {
            return _length;
        }

        inline __attribute__((always_inline)) size_t size() const {
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

        void write(const uint8_t *data, size_t len) {
            if (reserve(_length + len)) {
                memcpy(_buffer + _length, data, len);
                _length += len;
            }
        }

        inline __attribute__((always_inline)) void write(uint8_t data) {
            if (reserve(_length + 1)) {
                _buffer[_length++] = data;
            }
        }

        inline __attribute__((always_inline)) void write(uint8_t data1, uint8_t data2) {
            if (reserve(_length + 2)) {
                _buffer[_length++] = data1;
                _buffer[_length++] = data2;
                //auto ptr = _buffer + _length;
                //*ptr++ = data1;
                //*ptr++ = data2;
                //_length += 2;
            }
        }

        inline __attribute__((always_inline)) void writeWord(uint16_t data) {
            if (reserve(_length + 2)) {
                auto ptr = _buffer + _length;
                *reinterpret_cast<uint16_t *>(ptr) = data;
                _length += 2;
            }
        }

        inline __attribute__((always_inline)) void writeByteWord(uint8_t data1, uint16_t data2) {
            if (reserve(_length + 3)) {
                auto ptr = _buffer + _length;
                *ptr++ = data1;
                *reinterpret_cast<uint16_t *>(ptr) = data2;
                _length += 3;
            }
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

        inline __attribute__((always_inline)) bool reserve(size_t size) {
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

        inline __attribute__((always_inline)) void removeContent() {
            _length = 0;
        }

        //void remove(size_t index, size_t count) {
        //    if (index >= _length) {
        //        return;
        //    }
        //    if (count <= 0) {
        //        return;
        //    }
        //    if (count > _length - index) {
        //        count = _length - index;
        //    }
        //    _length = _length - count;
        //    if (_length - index) {
        //        memmove(_buffer + index, _buffer + index + count, _length - index);
        //    }
        //}

        inline __attribute__((always_inline)) uint16_t _alignSize(size_t size) const {
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

        Cache() = delete;
        Cache(const Cache& cache) = delete;

        Cache(Cache &&cache);
        Cache(coord_x_t width, scoord_y_t y = 0);
        ~Cache();

        Cache &operator =(const Cache &cache) = delete;
        Cache &operator =(Cache &&cache);

#if GFXCANVAS_MAX_CACHED_LINES > 1
        void allocBuffer();
        void freeBuffer();
#endif

        inline __attribute__((always_inline)) bool isY(scoord_y_t y) const {
            return _y == y;
        }

        inline __attribute__((always_inline)) scoord_y_t getY() const {
            return _y;
        }

        inline __attribute__((always_inline)) void setY(scoord_y_t y) {
            _read = 0;
            _write = 0;
            _y = y;
        }               
        // if the y position is changed, flags are cleared. in case the write flag is set, the cache must be written before
        inline __attribute__((always_inline)) bool isValid() const {
            return _y != INVALID;
        }


        inline __attribute__((always_inline)) bool hasWriteFlag() const {
            return _write;
        }          
        // indicates that the cache has not been written
        inline __attribute__((always_inline))  void setWriteFlag(bool value) {
            _write = value;
        }

        inline __attribute__((always_inline)) bool hasReadFlag() const {
            return _read;
        }           
        // indicates that the cache contains a copy
        inline __attribute__((always_inline)) void setReadFlag(bool value) {
            _read = value;
        }

        inline __attribute__((always_inline)) color_t *getBuffer() const
        {
            return _buffer;
        }

        inline __attribute__((always_inline)) void setPixel(scoord_x_t x, color_t color) {
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

        inline __attribute__((always_inline)) void clear(color_t fillColor)
        {
            _fillColor = fillColor;
            if (_buffer.length()) {
                _buffer.clear();
            }
            
        }

        inline __attribute__((always_inline)) coord_x_t getLength() const
        {
            return (coord_x_t)_buffer.length();
        }

        inline __attribute__((always_inline)) ByteBuffer &getBuffer()
        {
            return _buffer;
        }

        inline __attribute__((always_inline)) void setFillColor(color_t fillColor) {
            _fillColor = fillColor;
        }

        inline __attribute__((always_inline)) color_t getFillColor() const {
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
