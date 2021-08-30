/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "WriteableData.h"
#include "ConfigurationParameter.h"

namespace ConfigurationHelper {

    inline WriteableData::WriteableData(size_type length, ConfigurationParameter &parameter, Configuration &conf) :
        _buffer_start_words{},
        _buffer_end_word(0),
        _length(getParameterLength(parameter.getType(), length)),
        _is_string(parameter.getType() == ParameterType::STRING),
        _is_allocated(false)
    {
        if (_length > _buffer_length()) {
            _data = allocate(size() + 4, nullptr);
            _is_allocated = true;
        }

        if (parameter.hasData()) {
            auto &param = parameter._getParam();
            memcpy(data(), param._readable, std::min(_length, param.length()));
            free(param._readable);
            param._readable = nullptr;
        }
    }

    inline WriteableData::~WriteableData()
    {
        freeData();
    }

    inline size_type WriteableData::size() const
    {
        return _length + _is_string;
    }

    inline size_type WriteableData::length() const
    {
        return _length;
    }

    inline uint8_t *WriteableData::data()
    {
        return _length <= _buffer_length() ? _buffer_begin() : _data;
    }

    inline const uint8_t *WriteableData::data() const
    {
        return _length <= _buffer_length() ? _buffer_begin() : _data;
    }

    inline const uint8_t *WriteableData::begin() const
    {
        return data();
    }

    inline uint8_t *WriteableData::begin()
    {
        return data();
    }

    inline const uint8_t *WriteableData::end() const
    {
        return data() + size();
    }

    inline uint8_t *WriteableData::end()
    {
        return data() + size();
    }

    inline void WriteableData::setData(uint8_t *ptr, size_type length)
    {
        // free _data or clear _buffer and set new pointer
        freeData();
        __LDBG_printf("set data=%p length=%u", ptr, _length);
        _data = ptr;
        _length = length;
    }

    inline void WriteableData::freeData()
    {
        // free _data pointer or clear _buffer
        __LDBG_printf("free data=%p size=%u _is_allocated=%u", _data, size(), _is_allocated);
        if (_is_allocated) {
            free(_data);
            _is_allocated = false;
        }
        // clear _buffer and set _data to nullptr
        std::fill(_buffer_begin(), _buffer_end(), 0);
    }

    inline uint8_t *WriteableData::_buffer_begin() const
    {
        return (uint8_t *)&_buffer_start_words[0];
    }

    inline uint8_t *WriteableData::_buffer_end() const
    {
        return _buffer_begin() + _buffer_size();
    }

    inline size_t WriteableData::_buffer_size() const
    {
        return offsetof(WriteableData, _buffer_end_word) + sizeof(_buffer_end_word);
    }

    inline size_t WriteableData::_buffer_length() const
    {
        return _buffer_size() - 1;
    }

}
