/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if !_MSC_VER

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#include <debug_helper.h>
#if DEBUG_GFXCANVAS_MEM && 1
#include <debug_helper_enable_mem.h>
#else
#include <debug_helper_disable_mem.h>
#endif

#endif

namespace GFXCanvas {


#if 1
    template<typename data_type, typename size_type = uint8_t>
    class BufferTemplate
    {
    public:
        using data_type_ptr = data_type *;

        static constexpr size_t kAllocBlockSize = 8;
        static constexpr size_t kExtendSize = 8;
        static constexpr size_t kShrinkSize = 32;

        BufferTemplate() : _data(nullptr), _size(0), _capacity(0) {
        }

        // BufferTemplate(const BufferTemplate &copy) = delete;

        // BufferTemplate(const BufferTemplate &copy) : _data(nullptr), _size(0), _capacity(0) {
        //     *this = copy;
        // }

        BufferTemplate(const BufferTemplate &copy) : _data(copy._size ? new data_type[copy._size] : nullptr), _size(copy._size), _capacity(_size) {
            if (_data) {
                __LDBG_track_new(copy._size * sizeof(data_type));
                std::copy(copy.begin(), copy.end(), _data);
            }
            else {
                _size = 0;
                _capacity = 0;
            }
        }

        BufferTemplate(BufferTemplate &&move) noexcept :
            _data(std::exchange(move._data, nullptr)),
            _size(std::exchange(move._size, 0)),
            _capacity(std::exchange(move._capacity, 0)) {
        }

        BufferTemplate &operator=(BufferTemplate &&move) noexcept {
            if (_data) {
                __LDBG_track_delete(_capacity * sizeof(data_type));
                delete[] _data;
            }
            _data = std::exchange(move._data, nullptr);
            _size = std::exchange(move._size, 0);
            _capacity = std::exchange(move._capacity, 0);
            return *this;
        }

        BufferTemplate &operator=(const BufferTemplate &copy) {
            if (copy._size) {
                resize(copy._size);
                if (_capacity >= copy._size) {
                    _size = copy._size;
                    std::fill(std::copy(copy.begin(), copy.end(), _data), begin() + _capacity, 0);
                    return *this;
                }
            }
            resize(0);
            return *this;
        }

        ~BufferTemplate() {
            if (_data) {
                __LDBG_track_delete(_capacity * sizeof(data_type));
                delete[] _data;
            }
        }

        data_type_ptr data() {
            return _data;
        }

        data_type at(size_type n) const {
            return _data[n];
        }
        data_type &at(size_type n) {
            return _data[n];
        }

        data_type operator[](size_t n) const {
            return _data[n];
        }
        data_type &operator[](size_t n) {
            return _data[n];
        }

        data_type_ptr begin() {
            return _data;
        }
        const data_type_ptr begin() const {
            return _data;
        }
        data_type_ptr end() {
            return &_data[_size];
        }
        const data_type_ptr end() const {
            return &_data[_size];
        }

        void push(uint8_t data) {
            _check_capacity(1);
            _push_back(data);
        }

        void push_bb(uint8_t data1, uint8_t data2) {
            _check_capacity(2);
            _push_back(data1);
            _push_back(data2);
        }

        void push_bw(uint8_t data1, uint16_t data2) {
            _check_capacity(3);
            _push_back(data1);
            _push_back((data_type)data2);
            _push_back((data_type)(data2 >> 8));
        }

        void clear() {
            _size = 0;
        }

        void shrink_to_fit() {
            reserve(_size);
        }

        size_type length() const {
            return _size;
        }

        size_type size() const {
            return _capacity;
        }

        void setLength(size_type n) {
            _size = n;
        }

        void reserve(size_type n) {
            if (n == 0) {
                resize(0);
            }
            else if (n > _capacity) {
                resize((n + kExtendSize - 1) & ~(kAllocBlockSize - 1));
            }
            else if (n + kShrinkSize < _capacity) {
                resize((n + kAllocBlockSize - 1) & ~(kAllocBlockSize - 1));
            }
        }

        void resize(size_t n) {
            //Serial.printf("%p n=%u len=%u size=%u\n", this, n, _size, _capacity);
            if (n == 0) {
                if (_data) {
                    __LDBG_track_delete(_capacity * sizeof(data_type));
                    delete[] _data;
                    _data = nullptr;
                }
                _capacity = 0;
                _size = 0;
            }
            else {
                n = std::min(n, (size_t)std::numeric_limits<size_type>::max());
                data_type_ptr newData = new data_type[n];
                if (newData) {
                    __LDBG_track_new(n * sizeof(data_type));
                    if (_size) {
                        _size = std::min(_size, static_cast<size_type>(n));
                        std::copy(begin(), end(), newData);
                    }
                    if (_data) {
                        __LDBG_track_delete(_capacity * sizeof(data_type));
                        delete[] _data;
                    }
                    _data = newData;
                    _capacity = static_cast<size_type>(n);
                    std::fill(end(), begin() + _capacity, 0);
                }
            }
        }

    private:
        void _push_back(data_type data) {
            _data[_size++] = data;
        }

        void _check_capacity(size_type n) {
            if (_size + n > _capacity) {
                reserve(_size + n);
            }
        }

    private:
        data_type *_data;
        struct __attribute__packed__ {
            size_type _size;
            size_type _capacity;
        };
    };

    using ByteBuffer = BufferTemplate<uint8_t, ByteBufferSizeType>;

#else
    using ByteBufferBase = std::vector<uint8_t, __LDBG_track_allocator<uint8_t>>;

    class ByteBuffer : public ByteBufferBase {
    public:
        using ByteBufferBase::ByteBufferBase;

        static constexpr size_t kExpandBufferSize = 8;      // expand/shrink buffer by this amount
        static constexpr size_t kAllocBlockSize = 8;        // malloc block size

        static_assert(kExpandBufferSize >= 8, "too small");
        static_assert(kAllocBlockSize % 4 == 0, "not aligned");

        inline void push(uint8_t data) {
            checkCapacity();
            push_back(data);
        }

        // different name to avoid mixing it up with byte/word
        inline void push_bb(uint8_t data1, uint8_t data2) {
            checkCapacity();
            push_back(data1);
            push_back(data2);
        }

        inline void push_bw(uint8_t rle, ColorType color) {
            checkCapacity();
            push_back(rle);
            push_back(color);
            push_back(color >> 8);
        }

        uint8_t *begin() {
            return data();
        }
        const uint8_t *begin() const {
            return data();
        }
        uint8_t *end() {
            return &data()[length()];
        }
        const uint8_t *end() const {
            return &data()[length()];
        }

        inline size_t length() const {
            return ByteBufferBase::size();
        }


    private:
        // to avoid mixing up length() and size()
        size_t size() const {
            return capacity();
        }

        // allocation block size
        inline void checkCapacity() {
            size_t required = (ByteBufferBase::size() + kExpandBufferSize) & ~(kAllocBlockSize - 1);
            if (required > ByteBufferBase::capacity()) {
                reserve(required);
            }
        }

    public:
        // allocation shrink block size
        void shrink_to_fit() {
            size_t space = ByteBufferBase::capacity() - ByteBufferBase::size();
            if (space > kExpandBufferSize) {
                if (rand() % 16 == 0) {
                    ByteBufferBase::shrink_to_fit();
                }
                //ByteBufferBase::reserve((ByteBufferBase::size() + kExpandBufferSize - 1) & ~(kAllocBlockSize - 1));
            }
        }
    };
#endif
}

#include <debug_helper_disable_mem.h>
