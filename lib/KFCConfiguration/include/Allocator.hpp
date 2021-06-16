/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Allocator.h"
#include "ConfigurationHelper.h"

using namespace ConfigurationHelper;

inline uint8_t *Allocator::allocate(size_t size, uint8_t alignment)
{
    return allocate(size, nullptr, alignment);
}

inline void Allocator::deallocate(void *ptr)
{
    __LDBG_assert_printf(ptr != nullptr, "freeing nullptr");
    free(ptr);
}

inline void Allocator::release()
{
}

inline size_t Allocator::_get_aligned_size(size_t size, uint8_t alignment) const
{
    uint8_t tmp;
    if (alignment > (kAllocBlockSize + 1) && (tmp = (size % alignment))) {
        size += alignment - tmp;
    }
    return (size + kAllocBlockSize) & ~kAllocBlockSize;
}

inline void WriteableData::freeData()
{
    // free _data pointer or clear _buffer
    __LDBG_printf("free data=%p size=%u _is_allocated=%u", _data, size(), _is_allocated);
    if (_is_allocated) {
        _allocator.deallocate(_data);
        _is_allocated = false;
    }
    // clear _buffer and set _data to nullptr
    std::fill(_buffer_begin(), _buffer_end(), 0);
}
