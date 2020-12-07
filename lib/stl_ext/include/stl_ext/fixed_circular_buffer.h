/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "./stl_ext/non_std.h"

#if defined(ARDUINO)
#include <Print.h>
#endif

#pragma push_macro("new")
#undef new

namespace STL_STD_EXT_NAMESPACE_EX {

    template<class _Ty, size_t SIZE>
    class fixed_circular_buffer {
    public:
        using value_type = _Ty;
        using reference = _Ty &;
        using const_reference = const _Ty &;
        using pointer = _Ty *;
        using const_pointer = const _Ty *;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator_category = std::random_access_iterator_tag;
        using fixed_circular_buffer_type = fixed_circular_buffer<_Ty, SIZE>;
        using values_array_type = std::array<value_type, SIZE>;

        class const_iterator : public non_std::iterator<iterator_category, value_type, difference_type, pointer, reference> {
        public:
            const_iterator(const fixed_circular_buffer_type &buffer, const_pointer iterator) :
                _buffer(&const_cast<fixed_circular_buffer_type &>(buffer)),
                _iterator(const_cast<pointer>(iterator)) {
                //std::vector<int>
            }

            const_iterator operator+(const difference_type step) const {
                auto tmp = *this;
                tmp += step;
                return tmp;
            }

            const_iterator operator+(const difference_type step) {
                auto tmp = *this;
                tmp += step;
                return tmp;
            }

            difference_type operator-(const const_iterator &iter) const {
                return _iterator - iter._iterator;
            }

            const_iterator operator-(const difference_type step) const {
                auto tmp = *this;
                tmp -= step;
                return tmp;
            }

            const_iterator operator-(const difference_type step) {
                auto tmp = *this;
                tmp -= step;
                return tmp;
            }

            const_iterator &operator+=(const difference_type step) {
                _iterator += step;
                return *this;
            }

            const_iterator &operator-=(const difference_type step) {
                _iterator -= step;
                return *this;
            }

            const_iterator &operator++() {
                ++_iterator;
                return *this;
            }

            const_iterator &operator--() {
                --_iterator;
                return *this;
            }

            const_pointer operator->() const {
                return &_buffer->_values[offset()];
            }

            const_reference operator*() const {
                return _buffer->_values[offset()];
            }

            const_iterator &operator=(const const_iterator &iter) {
                _iterator = iter._iterator;
                return *this;
            }

            bool operator==(const const_iterator &iter) const {
                return _iterator == iter._iterator;
            }

            bool operator!=(const const_iterator &iter) const {
                return _iterator != iter._iterator;
            }

            size_type offset() const {
                return (_iterator - _buffer->_begin()) % _buffer->capacity();
            }

            bool isValid() const {
                return _iterator >= _buffer->_begin() + _buffer->_read_position && _iterator < _buffer->_begin() + _buffer->_count;
            }

        private:
            friend fixed_circular_buffer;

            fixed_circular_buffer_type *_buffer;
            pointer _iterator;
        };

        class iterator : public const_iterator {
        public:
            using const_iterator::operator*;
            using const_iterator::operator->;

            iterator(const fixed_circular_buffer_type &buffer, pointer iterator) : const_iterator(buffer, const_cast<const_pointer>(iterator)) {
            }

            pointer operator->() {
                return const_cast<pointer>(const_iterator::operator->());
            }

            reference operator*() {
                return const_cast<reference>(const_iterator::operator*());
            }
        };

        fixed_circular_buffer() : _count(0), _write_position(0), _read_position(0), _values() {
        }

        fixed_circular_buffer(const fixed_circular_buffer &buffer) :
            _count(buffer._count),
            _write_position(buffer._write_position),
            _read_position(buffer._read_position),
            _values(buffer._values)
        {
        }

        fixed_circular_buffer(fixed_circular_buffer &&buffer) noexcept :
            _count(std::exchange(buffer._count, 0)),
            _write_position(std::exchange(buffer._write_position, 0)),
            _read_position(std::exchange(buffer._read_position, 0)),
            _values(std::move(buffer._values))
        {
        }

        fixed_circular_buffer &operator=(const fixed_circular_buffer &buffer) {
            _count = buffer._count;
            _write_position = buffer._write_position;
            _read_position = buffer._read_position;
            _values = buffer._values;
            return *this;
        }

        fixed_circular_buffer &operator=(fixed_circular_buffer &&buffer) noexcept {
            _count = std::exchange(buffer._count, 0);
            _write_position = std::exchange(buffer._write_position, 0);
            _read_position = std::exchange(buffer._read_position, 0);
            _values = std::move(buffer._values);
            return *this;
        }

        fixed_circular_buffer_type copy_slice(const iterator first, const iterator last) const {
            fixed_circular_buffer_type tmp;
            tmp.copy(first, last);
            return tmp;
        }

        fixed_circular_buffer_type slice(iterator first, iterator last) {
            fixed_circular_buffer_type tmp;
            auto write_position = first.offset();
            auto count = std::distance(begin(), first) + _read_position;
            while (first != last) {
                tmp.push_back(std::move(*first));
                ++first;
            }
            _write_position = write_position;
            _count = count;
            return tmp;
        }

        // faster version of buffer = std::move(buffer.slice(first, last))
        // the objects removed are not destroyed until overwritten
        void shrink(const iterator first, const iterator last) {
            _read_position = first.offset();
            _write_position = last.offset();
            _count = std::distance(first, last) + _read_position;
        }

        template<class _Ta>
        void copy(_Ta first, _Ta last) {
            std::copy(first, last, std::back_inserter(*this));
        }

        inline __attribute__((__always_inline__))
        void push_back(value_type &&value) {
            emplace_back(std::move(value));
        }

        inline __attribute__((__always_inline__))
        void push_back(const value_type &value) {
            emplace_back(value);
        }

        inline __attribute__((__always_inline__))
        reference get_write_ref() {
            auto &tmp = _values[_write_position++];
            _write_position %= capacity();
            if (++_count - _read_position > capacity()) {
                _read_position++;
            }
            return tmp;
        }

        template<typename... Args>
        inline __attribute__((__always_inline__))
        void emplace_back(Args &&... args) {
            _values[_write_position].~value_type();
            ::new(static_cast<void *>(&_values[_write_position++])) value_type(std::forward<Args>(args)...);
            _write_position %= capacity();
            if (++_count - _read_position > capacity()) {
                _read_position++;
            }
        }

        inline __attribute__((__always_inline__))
        value_type pop_front() {
            auto tmp = std::exchange(front(), value_type());
            if (_read_position != _write_position) {
                _read_position++;
            }
            return tmp;
        }

        inline __attribute__((__always_inline__))
        value_type pop_back() {
            auto tmp = std::exchange(back(), value_type());
            if (_write_position == 0) {
                _write_position = capacity() - 1;
            }
            else {
                _write_position--;
            }
            if (--_count - _read_position > capacity()) {
                _read_position--;
            }

            return tmp;
        }

        inline __attribute__((__always_inline__))
        void clear() {
            _count = 0;
            _write_position = 0;
            _read_position = 0;
            std::fill(_values.begin(), _values.end(), value_type());
        }

        inline __attribute__((__always_inline__))
        size_type count() const {
            return _count;
        }

#if defined(ARDUINO)
        void printTo(Print &output, const iterator first, const iterator last, char separator = ',') {
            for (auto iter = first; iter != last; ++iter) {
                if (iter != first) {
                    output.print(separator);
                }
                output.print(*iter);
            }
        }
#endif

    public:

        inline __attribute__((__always_inline__))
        reference front() {
            return *begin();
        }

        inline __attribute__((__always_inline__))
        reference back() {
            return *std::prev(end());
            //return _values[_count - 1];
        }

        inline __attribute__((__always_inline__))
        const_reference front() const {
            return front();
        }

        inline __attribute__((__always_inline__))
        const_reference back() const {
            return back();
        }

        inline __attribute__((__always_inline__))
        iterator begin() {
            return iterator(*this, &_values.data()[_read_position]);
        }

        inline __attribute__((__always_inline__))
        iterator end() {
            return iterator(*this, &_values.data()[_count]);
        }

        inline __attribute__((__always_inline__))
        const_iterator begin() const {
            return const_iterator(*this, (const_pointer)&_values.data()[_read_position]);
        }

        inline __attribute__((__always_inline__))
            const_iterator end() const {
            return const_iterator(*this, (const_pointer)&_values.data()[_count]);
        }

        inline __attribute__((__always_inline__))
        const_iterator cbegin() const {
            return begin();
        }

        inline __attribute__((__always_inline__))
        const_iterator cend() const {
            return end();
        }

        inline __attribute__((__always_inline__))
        size_type size() const {
            return _count - _read_position;
        }

        inline __attribute__((__always_inline__))
        bool empty() const {
            return size() == 0;
        }

        inline __attribute__((__always_inline__))
        constexpr size_type capacity() const {
            return (size_type)SIZE;
        }

    public:
        values_array_type &_values_array() {
            return _values;
        }

    private:
        inline __attribute__((__always_inline__))
        constexpr const_pointer _begin() const {
            return (const_pointer)&_values[0];
        }

        inline __attribute__((__always_inline__))
        constexpr const_pointer _end() const {
            return (const_pointer)&_values[SIZE];
        }

    private:
        size_type _count;
        size_type _write_position;
        size_type _read_position;
        values_array_type _values;
    };

}

#pragma pop_macro("new")
