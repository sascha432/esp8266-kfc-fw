/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "MicrosTimer.h"

#if _CRTDBG_MAP_ALLOC
#pragma push_macro("new")
#undef new
#endif

class BoolMutex {
public:
    BoolMutex(const BoolMutex &) = delete;
    BoolMutex &operator=(const BoolMutex &) = delete;

    BoolMutex(bool locked = false) : _locked(locked) {}
    BoolMutex(BoolMutex &&move) noexcept : _locked(true) {
        *this = std::move(move);
    }
    BoolMutex &operator=(BoolMutex &&move) noexcept {
        noInterrupts();
        _locked = std::exchange(move._locked, false);
        interrupts();
        return *this;
    }

    bool try_lock() {
        noInterrupts();
        if (_locked == false) {
            _locked = true;
            interrupts();
            return true;
        }
        interrupts();
        return false;
    }

    void lock() {
        _locked = true;
    }

    void unlock() {
        _locked = false;
    }

private:
    volatile bool _locked;
};

template<class T, size_t SIZE, class mutex_type = BoolMutex>
class FixedCircularBuffer {
public:
    using buffer_type = FixedCircularBuffer<T, SIZE, mutex_type>;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using value_type = T;
    using size_type = size_t;
    using difference_type = typename std::make_signed<size_type>::type;
    using iterator_category = std::random_access_iterator_tag;

    class iterator : public std::iterator<iterator_category, value_type, difference_type, pointer, reference> {
    public:
        iterator(buffer_type &buffer, pointer iterator) : _buffer(&buffer), _iterator(iterator) {
        }

        iterator(const buffer_type &buffer, const_pointer iterator) : _buffer(&const_cast<buffer_type &>(buffer)), _iterator(const_cast<pointer>(iterator)) {
        }

        iterator &operator=(const iterator iter) {
            _iterator = iter._iterator;
            return *this;
        }

        const iterator operator+(difference_type step) const {
            auto tmp = *this;
            tmp += step;
            return tmp;
        }

        iterator operator+(difference_type step) {
            auto tmp = *this;
            tmp += step;
            return tmp;
        }

        difference_type operator-(const iterator iter) const {
            return _iterator - iter._iterator;
        }

        iterator operator-(difference_type step) const {
            auto tmp = *this;
            tmp -= step;
            return tmp;
        }

        iterator operator-(difference_type step) {
            auto tmp = *this;
            tmp -= step;
            return tmp;
        }

        iterator &operator+=(difference_type step) {
            _iterator += step;
            return *this;
        }

        iterator &operator-=(difference_type step) {
            _iterator -= step;
            return *this;
        }

        iterator &operator++() {
            ++_iterator;
            return *this;
        }

        iterator &operator--() {
            --_iterator;
            return *this;
        }

        const reference operator*() const {
            return _buffer->_values[offset()];
        }

        reference operator*() {
            return _buffer->_values[offset()];
        }

        bool operator==(iterator iter) const {
            return _iterator == iter._iterator;
        }

        bool operator!=(iterator iter) const {
            return _iterator != iter._iterator;
        }

        size_type offset() const {
            return (_iterator - _buffer->_begin()) % _buffer->capacity();
        }

        bool isValid() const {
            return _iterator >= _buffer->_begin() + _buffer->_read_position && _iterator < _buffer->_begin() + _buffer->_count;
        }

    private:
        friend FixedCircularBuffer;

        buffer_type *_buffer;
        pointer _iterator;
    };

    FixedCircularBuffer() : _count(0), _write_position(0), _read_position(0), _values() {
    }

    FixedCircularBuffer(const FixedCircularBuffer &buffer) :
        _count(buffer._count),
        _write_position(buffer._write_position),
        _read_position(buffer._read_position),
        _values(buffer._values)
    {
    }

    FixedCircularBuffer(FixedCircularBuffer &&buffer) noexcept :
        _count(std::exchange(buffer._count, 0)),
        _write_position(std::exchange(buffer._write_position, 0)),
        _read_position(std::exchange(buffer._read_position, 0)),
        _values(std::move(buffer._values)),
        _mutex(std::move(buffer._mutex))
    {
    }

    FixedCircularBuffer &operator=(const FixedCircularBuffer &buffer) {
        _count = buffer._count;
        _write_position = buffer._write_position;
        _read_position = buffer._read_position;
        _values = buffer._values;
        return *this;
    }

    buffer_type &operator=(buffer_type &&buffer) noexcept {
        _count = std::exchange(buffer._count, 0);
        _write_position = std::exchange(buffer._write_position, 0);
        _read_position = std::exchange(buffer._read_position, 0);
        _values = std::move(buffer._values);
        _mutex = std::move(buffer._mutex);
        return *this;
    }

    buffer_type copy_slice(const iterator first, const iterator last) const {
        buffer_type tmp;
        tmp.copy(first, last);
        return tmp;
    }

    buffer_type slice(iterator first, iterator last) {
        buffer_type tmp;
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
    void shrink(const iterator first, const iterator last) {
        _read_position = first.offset();
        _write_position = last.offset();
        _count = std::distance(first, last) + _read_position;
    }

    template<class Ti>
    void copy(Ti first, Ti last) {
        std::copy(first, last, std::back_inserter(*this));
    }

    void push_back(T &&value) {
        emplace_back(std::move(value));
    }

    void push_back(const T &value) {
        emplace_back(value);
    }

    template<typename... Args>
    void emplace_back(Args &&... args) {
        new(&_values[_write_position++]) T(std::forward<Args>(args)...);
        _write_position %= capacity();
        if (++_count - _read_position > capacity()) {
            _read_position++;
        }
    }

    void push_back_no_block(T &&value) {
        if (_mutex.try_lock() == false) {
            emplace_back(std::move(value));
            _mutex.unlock();
        }
    }

    bool push_back_timeout(T &&value, uint32_t timeoutMicros) {
        if (_mutex.try_lock() == false) {
            uint32_t start = micros();
            while (_mutex.try_lock() == false) {
                if (get_time_diff(start, micros()) > timeoutMicros) {
                    return false;
                }
            }
        }
        emplace_back(std::move(value));
        _mutex.unlock();
        return true;
    }

    value_type pop_front() {
        return _values[_read_position++];
    }

    bool try_lock() {
        return _mutex.try_lock();
    }

    void lock() {
        _mutex.lock();
    }

    void unlock() {
        _mutex.unlock();
    }

    mutex_type &get_mutex() {
        return _mutex;
    }

    void clear() {
        _count = 0;
        _write_position = 0;
        _read_position = 0;
        _mutex.unlock();
    }

    size_type count() const {
        return _count;
    }

    void printTo(Print &output, const iterator first, const iterator last, char separator = ',') {
        for(auto iter = first; iter != last; ++iter) {
            if (iter != first) {
                output.print(separator);
            }
            output.print(*iter);
        }
    }

public:

    reference front() {
        return _values[_read_position];
    }

    reference back() {
        return _values[_count - 1];
    }

    const_reference front() const {
        return front();
    }

    const_reference back() const {
        return back();
    }

    iterator begin() {
        return iterator(*this, &_values.data()[_read_position]);
    }

    iterator end() {
        return iterator(*this, &_values.data()[_count]);
    }

    iterator begin() const {
        return iterator(*this, &_values.data()[_read_position]);
    }

    iterator end() const {
        return iterator(*this, &_values.data()[_count]);
    }

    iterator cbegin() const {
        return begin();
    }

    iterator cend() const {
        return end();
    }

    size_type size() const {
        return _count - _read_position;
    }

    constexpr size_type capacity() const {
        return (size_type)SIZE;
    }

private:
    constexpr const_pointer _begin() const {
        return &_values[0];
    }

    constexpr const_pointer _end() const {
        return &_values[SIZE];
    }

private:
    size_type _count;
    size_type _write_position;
    size_type _read_position;
    std::array<value_type, SIZE> _values;
    mutex_type _mutex;
};

#if _CRTDBG_MAP_ALLOC
#pragma pop_macro("new")
#endif
