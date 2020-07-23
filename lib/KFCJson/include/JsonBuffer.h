/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonValue.h"

// JsonBuffer can be used to print the entire JSON object with a small buffer
// small buffers have a certain performance penalty, recommended size is the longest stored value * 2
//
//JsonBuffer jBuf(&jsonObject);
//uint8_t buf[8];
//size_t len;
//while (len = jBuf.fillBuffer(buf, sizeof(buf))) {
//    printf("%*.*s", len, len, buf);
//}
//printf("\n");

class JsonPrint : public Print {
public:
    JsonPrint(uint8_t *buffer, size_t size);

    virtual size_t write(uint8_t data);
    virtual size_t write(const uint8_t *buffer, size_t size);

    inline size_t getSize() const {
        return _size;
    }
    inline int getLength() const {
        return _size - _space;
    }
    inline bool isOverflow() const {
        return _overflow;
    }
    inline bool isFull() const {
        return !_space;
    }
    inline void setSkip(size_t skip) {
        _skip = skip;
    }

private:
    uint8_t *_ptr;
    uint8_t *_buffer;
    size_t _size;
    size_t _space;
    size_t _skip;
    bool _overflow;
};

class JsonArray;
class JsonObject;
class JsonUnnamedArray;
class JsonUnnamedObject;

class JsonBuffer {
public:
//    typedef int16_t Stack;      // save some RAM

    class Stack {
    public:
        Stack(AbstractJsonValue &object) {
            _position = object.hasName() ? -1 : 0;
        }
        Stack &operator ++() {
            _position++;
            return *this;
        }
        AbstractJsonValue *getObject(AbstractJsonValue *object) {
            if (position() <= 0) {
                return object->getVector()->front();
            }
            else {
                return (*object->getVector())[position()];
            }
        }
        inline int position() const {
            return _position;
        }
    private:
        int16_t _position;
    };
//class Stack {
//public:
//    Stack(AbstractJsonValue &object) : _value(&object) {
//            vectorPosition = object.hasName() ? -1 : 0;
//    }
//    inline AbstractJsonValue &value() {
//        return *_value;
//    }
//    Stack &operator ++() {
//        vectorPosition++;
//        return *this;
//    }
//    inline int position() const {
//        return vectorPosition;
//    }
//private:
//    AbstractJsonValue *_value;
//    int vectorPosition;
//};

    typedef std::vector<JsonBuffer::Stack> JsonPrintStack;

private:
    JsonBuffer(AbstractJsonValue &object) : _object(object) {
        reset();
    }
public:
    JsonBuffer(const JsonObject &object) : JsonBuffer(reinterpret_cast<AbstractJsonValue &>(const_cast<JsonObject &>(object))) {
    }
    JsonBuffer(const JsonUnnamedObject &object) : JsonBuffer(reinterpret_cast<AbstractJsonValue &>(const_cast<JsonUnnamedObject &>(object))) {
    }
    JsonBuffer(const JsonArray &object) : JsonBuffer(reinterpret_cast<AbstractJsonValue &>(const_cast<JsonArray &>(object))) {
    }
    JsonBuffer(const JsonUnnamedArray &object) : JsonBuffer(reinterpret_cast<AbstractJsonValue &>(const_cast<JsonUnnamedArray &>(object))) {
    }

    void reset();
    size_t fillBuffer(uint8_t *buf, size_t size);

private:
    bool isBufferFull(JsonPrint &print, bool advance);
    AbstractJsonValue *getObject(uint8_t skip = 1);

    JsonPrintStack _stack;
    AbstractJsonValue &_object;
    int _writePosition;
};
