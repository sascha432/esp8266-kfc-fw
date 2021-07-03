/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ConfigurationHelper.h"

namespace ConfigurationHelper {

    class WriteableData {
    public:

        WriteableData(size_type length, ConfigurationParameter &parameter, Configuration &conf);
        ~WriteableData();

        void resize(size_type length, ConfigurationParameter &parameter, Configuration &conf);

        size_type size() const {
            return _length + _is_string;
        }

        size_type length() const {
            return _length;
        }

        uint8_t *data() {
            return _length <= _buffer_length() ? _buffer_begin() : _data;
        }

        const uint8_t *data() const {
            return _length <= _buffer_length() ? _buffer_begin() : _data;
        }

        const uint8_t *begin() const {
            return data();
        }

        uint8_t *begin() {
            return data();
        }

        const uint8_t *end() const {
            return data() + size();
        }

        uint8_t *end() {
            return data() + size();
        }

        void setData(uint8_t *ptr, size_type length);
        void freeData();

        struct {
            union {
                uint8_t *_data;
                uint16_t _buffer_start_words[6]; // use _buffer_begin()
            };
            uint16_t _buffer_end_word; // use _buffer_end() and _buffer_size()

            uint16_t _length : 11;
            uint16_t _is_string : 1;
            uint16_t _is_allocated : 1;
        };

        // to avoid packing we need to keep the union 32 bit aligned and create some helpers for buffer begin(), end() and sizeof()
        uint8_t *_buffer_begin() const {
            return (uint8_t *)&_buffer_start_words[0];
        }

        uint8_t *_buffer_end() const {
            return _buffer_begin() + _buffer_size();
        }

        size_t _buffer_size() const {
            return offsetof(WriteableData, _buffer_end_word) + sizeof(_buffer_end_word);
        }

        size_t _buffer_length() const {
            return _buffer_size() - 1;
        }

    };

    static constexpr size_t WriteableDataSize = sizeof(WriteableData);

    static_assert(WriteableDataSize == 16 || sizeof(void *) == 8/*platformide detedcts code as x64,, we cannot validate size*/, "check alignment");

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

}
