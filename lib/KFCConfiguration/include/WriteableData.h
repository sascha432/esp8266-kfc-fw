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

        size_type size() const;
        size_type length() const;
        uint8_t *data();
        const uint8_t *data() const;
        const uint8_t *begin() const;
        uint8_t *begin();
        const uint8_t *end() const;
        uint8_t *end();

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
        uint8_t *_buffer_begin() const;
        uint8_t *_buffer_end() const;
        size_t _buffer_size() const;
        size_t _buffer_length() const;

    };

    static constexpr size_t WriteableDataSize = sizeof(WriteableData);

    static_assert(WriteableDataSize == 16 || sizeof(void *) == 8/*platformide detedcts code as x64,, we cannot validate size*/, "check alignment");

}
