/**
* Author: sascha_lammers@gmx.de
*/

#include "Allocator.h"
#include "ConfigurationParameter.h"
#include "Allocator.hpp"

ConfigurationHelper::Allocator ConfigurationHelper::_allocator;

using namespace ConfigurationHelper;

uint8_t *Allocator::allocate(size_t size, size_t *realSize, uint8_t alignment)
{
    __LDBG_assert_printf(size != 0, "allocating size=%u", size);
    size = std::max(1U, size);
    auto ptr = reinterpret_cast<uint32_t *>(malloc(size = _get_aligned_size(size, alignment)));
    __LDBG_assert_printf(ptr != nullptr, "malloc=%u returned=%p", size, ptr);
    if (!ptr) {
        if (realSize) {
            *realSize = 0;
        }
        return nullptr;
    }
    if (realSize) {
        *realSize = size;
    }
    static_assert(kAllocBlockSize == 3 || kAllocBlockSize == 7 || kAllocBlockSize == 15, "cannot fill as uint32_t");
    std::fill_n(ptr, size / sizeof(uint32_t), 0);
    return (uint8_t *)ptr;
}

void Allocator::allocate(size_t size, ConfigurationParameter &parameter, uint8_t alignment)
{
    __LDBG_assert_printf(size != 0, "allocating size=%u", size);
#if 1
    __LDBG_assert_printf(parameter.hasData() == false, "has_data=%u is_writable=%u _readable=%p", parameter.hasData(), parameter.isWriteable(), parameter._getParam()._readable);
    if (parameter.hasData()) {
        deallocate(parameter);
    }
#else
    __LDBG_assert_panic(parameter.hasData() == false, "has_data=%", parameter.hasData());
#endif
    auto &param = parameter._getParam();
    __LDBG_assert_printf(size >= param.size(), "size=%u too small param.size=%u", size, param.size());
    param._readable = allocate(size, alignment);
}

void Allocator::deallocate(ConfigurationParameter &parameter)
{
    auto &param = parameter._getParam();
    if (param.isWriteable()) {
        //param._length = param._writeable->length();
        delete param._writeable;
        param._writeable = nullptr;
        param._is_writeable = false;
    }
    if (param._readable) {
        deallocate(param._readable);
        param._readable = nullptr;
    }
}
