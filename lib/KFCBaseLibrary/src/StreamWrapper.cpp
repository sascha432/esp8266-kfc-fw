/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include "StreamWrapper.h"

StreamWrapper::StreamWrapper() {
    setInput(nullptr);
}

StreamWrapper::StreamWrapper(Stream *output, Stream *input) {
    add(output);
    setInput(input);
}

StreamWrapper::~StreamWrapper() {
    clear();
}

void StreamWrapper::setInput(Stream *input) {
    _input = input;
}

Stream *StreamWrapper::getInput() {
    return _input;
}

void StreamWrapper::add(Stream *output) {
    _children.push_back(output);
}

void StreamWrapper::remove(Stream *output) {
    _children.erase(std::remove(_children.begin(), _children.end(), output), _children.end());
}

void StreamWrapper::clear() {
    _children.clear();
}

void StreamWrapper::replace(Stream *output, bool input) {
    clear();
    add(output);
    if (input) {
        setInput(output);
    }
}

Stream *StreamWrapper::first() {
    if (_children.size()) {
        return _children.front();
    }
    return nullptr;
}

Stream *StreamWrapper::last() {
    if (_children.size()) {
        return _children.back();
    }
    return nullptr;
}

size_t StreamWrapper::count() const {
    return _children.size();
}

streamWrapperVector &StreamWrapper::getChildren() {
    return _children;
}

int StreamWrapper::available() {
    return _input ? _input->available() : false;
}

int StreamWrapper::read() {
    return _input ? _input->read() : -1;
}

int StreamWrapper::peek() {
    return _input ? _input->peek() : -1;
}

size_t StreamWrapper::readBytes(char *buffer, size_t length) {
    return _input ? _input->readBytes(buffer, length) : 0;
}

size_t StreamWrapper::write(uint8_t data) {
    // Serial.printf_P(PSTR("StreamWrapper::write(%u)\n"), data);
    size_t res = 0;
    for(auto child: _children) {
        res = child->write(data);
    }
    return res;
}

size_t StreamWrapper::write(const uint8_t *buffer, size_t size) {
    // Serial.printf_P(PSTR("StreamWrapper::write(%p, %d)\n"), buffer, size);
    size_t res = 0;
    for(auto child: _children) {
        res = child->write(buffer, size);
    }
    return res;
}
