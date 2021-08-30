/**
* Author: sascha_lammers@gmx.de
*/

#include "Configuration.hpp"

using namespace ConfigurationHelper;

void WriteableData::resize(size_type newLength, ConfigurationParameter &parameter, Configuration &conf)
{
    if (newLength == length()) {
        return;
    }

    auto &param = parameter._getParam();
    // update string length
    if (param.isString()) {
        __LDBG_printf("len=%u strlen=%u writeable_length=%u", param._length, strlen(param.string()), length());
        param._length = strlen(param.string());
        _length = param._length;
    }
    __LDBG_printf("new_length=%u length=%u data=%p", newLength, length(), data());

    size_t newSize = param.sizeOf(newLength);
    if (newLength > _buffer_length()) {
        // allocate new block
        size_t realSize;
        auto ptr = allocate(newSize + 4, &realSize);
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
        free(ptr);
        _is_allocated = false;
        return;
    }

    // the data is already in _buffer
    // fill it with 0 after newLength
    std::fill(_buffer_begin() + newLength, _buffer_end(), 0);
}

