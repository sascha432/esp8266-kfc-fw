/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasBufferPool.h"

#include <debug_helper.h>

#if 0

namespace GFXCanvas {

    // read only buffer
    template<typename data_type, typename size_type>
    class BufferTemplate
    {
    public:
        using data_type_ptr = data_type *;

        inline BufferTemplate() :
            _data(nullptr),
            _size(0),
            _capacity(0)
        {
        }

        BufferTemplate(const BufferTemplate &copy) :
            _data(copy._size ? new data_type[copy._size] : nullptr),
            _size(copy._size),
            _capacity(_size)
        {
            if (_data) {
                std::copy(copy.begin(), copy.end(), begin());
            }
            else {
                _size = 0;
                _capacity = 0;
            }
        }

        inline BufferTemplate(BufferTemplate &&move) noexcept :
            _data(std::exchange(move._data, nullptr)),
            _size(std::exchange(move._size, 0)),
            _capacity(std::exchange(move._capacity, 0)) {
        }

        // BufferTemplate &operator=(BufferPool &&pool) noexcept {
        //     __releaseData();
        //     #if 0
        //         auto &b = pool.getTemp();
        //         _fp_size = b.length();
        //         _capacity = b.size();
        //         b.move(&_data);
        //     #else
        //         pool.moveTempTo(*this);
        //     #endif
        //     return *this;
        // }

        BufferTemplate &operator=(BufferTemplate &&move) noexcept {
            __releaseData();
            _data = std::exchange(move._data, nullptr);
            _size = std::exchange(move._size, 0);
            _capacity = std::exchange(move._capacity, 0);
            return *this;
        }

        BufferTemplate &operator=(const BufferTemplate &copy) {
            __releaseData();
            *this = std::move(BufferTemplate(copy));
            return *this;
        }

        ~BufferTemplate() {
            if (_data && _capacity == 0) {
                BufferPool::getInstance().remove(this);
            }
            else {
                __releaseData();
            }
        }

        inline void __releaseData()
        {
            if (_data != nullptr) {
                if (_capacity != 0) {
                    delete[] _data;
                    _data = nullptr;
                }
            }
        }

        inline data_type_ptr data() {
            return _data;
        }

        inline data_type at(size_type n) const {
            return _data[n];
        }
        inline data_type &at(size_type n) {
            return _data[n];
        }

        inline data_type operator[](size_t n) const {
            return _data[n];
        }
        inline data_type &operator[](size_t n) {
            return _data[n];
        }

        inline data_type_ptr begin() {
            return _data;
        }
        inline const data_type_ptr begin() const {
            return _data;
        }
        inline data_type_ptr end() {
            return &_data[_size];
        }
        inline const data_type_ptr end() const {
            return &_data[_size];
        }

        inline void clear() {
            _size = 0;
        }

        inline size_type length() const {
            return _size;
        }

        inline size_type size() const {
            return _capacity;
        }

    private:
        friend BufferPool;

    private:
        data_type *_data;
        size_type _size;
        size_type _capacity;
    };
}

#endif
