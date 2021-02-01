/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonBuffer.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


JsonPrint::JsonPrint(uint8_t *buffer, size_t size) {
    _buffer = buffer;
    _ptr = buffer;
    _size = size;
    _space = size;
    _skip = 0;
    _overflow = false;
}

size_t JsonPrint::write(uint8_t data) {
    if (_skip) {
        _skip--;
        return 1;
    }
    else if (_space) {
        *_ptr++ = data;
        _space--;
        return 1;
    }
    _overflow = true;
    return 0;
}

size_t JsonPrint::write(const uint8_t *buffer, size_t size) {
    if (_overflow) {
        return 0;
    }
    if (_skip >= size) {
        _skip -= size;
        return _size;
    }
    auto count = size;
    if (_skip) {
        count -= _skip;
        buffer += _skip;
        _skip = 0;
    }
    if (count > _space) {
        _overflow = true;
        count = _space;
        _space = 0;
        memcpy(_ptr, buffer, count);
        _ptr += count;
        return count;
    }
    else {
        _space -= count;
        memcpy(_ptr, buffer, count);
        _ptr += count;
    }
    return size;
}


void JsonBuffer::reset() {
    _writePosition = 0;
    _stack.clear();
    _stack.reserve(8); // reserve space for 8 nested levels = 16 byte = min. allocation size for ESP8266
    _stack.push_back(Stack(_object));
}

size_t JsonBuffer::fillBuffer(uint8_t * buf, size_t size) {
    if (!_stack.size()) {
        return 0;
    }
    JsonPrint print(buf, size);
    print.setSkip(_writePosition);

    auto object = getObject();

recursiveCall:
    do {
        auto &stack = _stack.back();
        auto vector = object->getVector();
        if (vector) {
            if (stack.position() == -1) { // print key of variant
                print.write('"');
                if (isBufferFull(print, false)) {
                    return print.getSize();
                }
                object->getName()->printTo(print);
                print.write('"');
                print.write(':');
                if (isBufferFull(print, true)) {
                    return print.getSize();
                }
            }
            else {
                // print array or object
                auto iter = vector->begin() + stack.position();
                if (stack.position() == 0) {
                    print.write(object->isObject() ? '{' : '[');
                    if (isBufferFull(print, false)) {
                        return print.getSize();
                    }
                }
                while (iter != vector->end()) {
                    auto &value = *(*iter);
                    if (value.getVector()) { // check if variant is array or vector
                        _writePosition = -print.getLength();
                        _stack.push_back(Stack(value));
                        object = &value;
                        goto recursiveCall;
                    }
                    value.printTo(print);
                    if (isBufferFull(print, false)) {
                        return print.getSize();
                    }
                    if (iter + 1 != vector->end()) {
                        print.write(',');
                        if (isBufferFull(print, true)) {
                            return print.getSize();
                        }
                    }
                    ++iter;
                }

                print.write(object->isObject() ? '}' : ']');
                if (isBufferFull(print, false)) {
                    return print.getSize();
                }
                if (_stack.size() == 1) {
                    _stack.clear(); // last item is done
                    return print.getLength();
                }
                auto &prev = _stack.at(_stack.size() - 2); // check if the last array or objects is done
                object = getObject(2);
                if (prev.position() + 1 < (int)object->getVector()->size()) {
                    if (print.write(',') == 0) {
                        if (isBufferFull(print, false)) {
                            return print.getSize();
                        }
                    }
                }
                _stack.erase(_stack.end() - 1);
                ++_stack.back();
                _writePosition = -print.getLength();
            }
        }
    } while (_stack.size());

    return print.getLength();
}

bool JsonBuffer::isBufferFull(JsonPrint &print, bool advance) {
    auto &stack = _stack.back();
    if (print.isOverflow()) {
        _writePosition += print.getSize(); // buffer overflow, continue at write position
        return true;
    }
    if (advance) {
        _writePosition = -print.getLength(); // advance to the next item
        ++stack;
    }
    if (print.isFull()) {
        _writePosition += print.getSize(); // buffer full, store write position
        return true;
    }
    return false;
}

AbstractJsonValue *JsonBuffer::getObject(uint8_t skip) {
    AbstractJsonValue *object = &_object;
    auto end = _stack.end() - skip;
    for(auto iter = _stack.begin(); iter != end; ++iter) {
        object = iter->getObject(object);
    }
    return object;
}
