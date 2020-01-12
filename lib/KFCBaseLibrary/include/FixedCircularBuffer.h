/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "MicrosTimer.h"

template<class T, size_t SIZE>
class FixedCircularBuffer {
public:
    class iterator {
    public:
        iterator(const FixedCircularBuffer<T, SIZE>& buffer, const T* iterator) : _buffer(buffer), _iterator(iterator) {
        }

        iterator& operator=(const iterator &iter) {
            _iterator = iter._iterator;
            return *this;
        }

        iterator operator+(int step) {
            auto tmp = *this;
            tmp += step;
            return tmp;
        }
        iterator operator-(int step) {
            auto tmp = *this;
            tmp -= step;
            return tmp;
        }

        iterator& operator+=(int step) {
            _iterator += step;
            return *this;
        }
        iterator& operator-=(int step) {
            _iterator -= step;
            return *this;
        }
        iterator& operator++() {
            ++_iterator;
            return *this;
        }
        iterator& operator--() {
            --_iterator;
            return *this;
        }

        T& operator*() {
            return *(T *)& _buffer._values[offset()];
        }

        bool operator==(const iterator& iter) {
            return _iterator == iter._iterator;
        }

        bool operator!=(const iterator& iter) {
            return _iterator != iter._iterator;
        }

        size_t offset() const {
            return (_iterator - &_buffer._values[0]) % SIZE;
        }

        bool isValid() const {
           return _iterator >= _buffer._beginPtr() + _buffer._read_position && _iterator < _buffer._beginPtr() + _buffer._count;
        }

    private:
        friend FixedCircularBuffer;

        const FixedCircularBuffer<T, SIZE>& _buffer;
        const T* _iterator;
    };

    FixedCircularBuffer() : _count(0), _position(0), _read_position(0), _locked(false) {
    }

    FixedCircularBuffer(FixedCircularBuffer&& buffer) {
        *this = std::move(buffer);
    }

    FixedCircularBuffer& operator=(FixedCircularBuffer&& buffer) {
        _count = buffer._count;
        _position = buffer._position;
        _read_position = buffer._read_position;
        memcpy(_values, buffer._values, SIZE * sizeof(*_values));
        buffer.clear();
        _locked = false;
        return *this;
    }

    FixedCircularBuffer slice(const iterator& first, const iterator& last) {
        FixedCircularBuffer tmp;
        for (auto iterator = first; iterator != last; ++iterator) {
            tmp.push_back(*iterator);
        }
        return tmp;
    }

    static int distance(const iterator& iter1, const iterator& iter2) {
        return iter2._iterator - iter1._iterator;
    }

    inline __attribute__((always_inline)) void push_back(T value) {
        _values[_position++] = value;
        _position %= SIZE;
        _count++;
        if (_count - _read_position > SIZE) {
            _read_position++;
        }
    }

    inline __attribute__((always_inline)) void push_back_no_block(T value) {
        if (!_locked) {
            push_back(value);
        }
    }

    bool push_back_timeout(T value, uint32_t timeoutMicros) {
        if (_locked) {
            uint32_t start = micros();
            while(_locked) {
                if (get_time_diff(start, micros()) > timeoutMicros) {
                    return false;
                }
            }
        }
        push_back(value);
        return true;
    }

    T pop_front() {
        T value = front();
        _read_position++;
        return value;
    }

    void lock() {
        _locked = true;
    }

    void unlock() {
        _locked = false;
    }

    bool isLocked() const {
        return _locked;
    }

    void clear() {
        _count = 0;
        _position = 0;
        _read_position = 0;
        _locked = false;
    }

    size_t getCount() const {
        return _count;
    }

    void printTo(Print &output, const iterator &first, const iterator &last, char separator = ',') {
        for(auto iter = first; iter != last; ++iter) {
            if (iter != first) {
                output.print(separator);
            }
            output.print(*iter);
        }
    }

public:

    T& front() {
        return *begin();
    }

    T& back() {
        return *(end() - 1);
    }

    iterator begin() const {
        return iterator(*this, &_values[_read_position]);
    }
    iterator end() const {
        return iterator(*this, &_values[_count]);
    }

    size_t size() const {
        return _count - _read_position;
    }

    constexpr size_t capacity() const {
        return SIZE;
    }

private:
    const T* _beginPtr() const {
        return &_values[0];
    }

    const T* _endPtr() const {
        return &_values[SIZE];
    }

public:
    size_t _count;
    size_t _position;
    size_t _read_position;
    bool _locked;
    T _values[SIZE];
};
