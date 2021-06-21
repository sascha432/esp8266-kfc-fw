/**
* Author: sascha_lammers@gmx.de
*/

#include "ConfigurationHelper.h"
#include "WriteableData.h"
#include "Allocator.h"
#include "ConfigurationParameter.h"

#include "Allocator.hpp"

using namespace ConfigurationHelper;

WriteableData::WriteableData(size_type length, ConfigurationParameter &parameter, Configuration &conf) :
    _buffer_start_words{},
    _buffer_end_word(0),
    _length(getParameterLength(parameter.getType(), length)),
    _is_string(parameter.getType() == ParameterType::STRING),
    _is_allocated(false)
{
    if (_length > _buffer_length()) {
        _data = _allocator.allocate(size() + 4);
        _is_allocated = true;
    }

    if (parameter.hasData()) {
        auto &param = parameter._getParam();
        memcpy(data(), param._readable, std::min(_length, param.length()));
        _allocator.deallocate(param._readable);
        param._readable = nullptr;
    }
}

WriteableData::~WriteableData()
{
    freeData();
}

void WriteableData::resize(size_type newLength, ConfigurationParameter &parameter, Configuration &conf)
{
    if (newLength == length()) {
        return;
    }

    auto &param = parameter._getParam();
    // update string length
    if (param.isString()) {
        __DBG_printf("len=%u strlen=%u writeable_length=%u", param._length, strlen(param.string()), length());
        param._length = strlen(param.string());
        _length = param._length;
    }
    __DBG_printf("new_length=%u length=%u data=%p", newLength, length(), data());

    size_t newSize = param.sizeOf(newLength);
    if (newLength > _buffer_length()) {
        // allocate new block
        size_t realSize;
        auto ptr = _allocator.allocate(newSize + 4, &realSize);
        // copy previous data and zero fill the rest
        std::fill(std::copy_n(param._writeable->data(), std::min(length(), newLength), ptr), ptr + realSize, 0);

        // free old data and set new pointer
        setData(ptr, newLength);
        return;
    }

    // data fits into _buffer
    if (_is_allocated) {
        // we need to create a copy of the _data pointer since it is sharing _buffer
        auto ptr = _data;
        // copy existing data and zero fill
        std::fill(std::copy_n(ptr, std::min(length(), newLength), _buffer_begin()), _buffer_end(), 0);
        _length = newLength;
        // free saved pointer
        _allocator.deallocate(ptr);
        _is_allocated = false;
        return;
    }

    // the data is already in _buffer
    // fill it with 0 after newLength
    std::fill(_buffer_begin() + newLength, _buffer_end(), 0);
}

void WriteableData::setData(uint8_t *ptr, size_type length)
{
    // free _data or clear _buffer and set new pointer
    freeData();
    __LDBG_printf("set data=%p length=%u", ptr, _length);
    _data = ptr;
    _length = length;
}

