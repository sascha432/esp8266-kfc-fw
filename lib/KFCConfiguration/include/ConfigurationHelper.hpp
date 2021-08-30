/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ConfigurationHelper.h"
#include "ConfigurationParameter.h"

namespace ConfigurationHelper {

    inline uint8_t *allocate(size_t size, size_t *realSize)
    {
        __LDBG_assert_printf(size != 0, "allocating size=%u", size);
        size = std::max(1U, size);
        size = (size + 7) & ~7;
        auto ptr = reinterpret_cast<uint32_t *>(malloc(size));
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
        std::fill_n(ptr, size / sizeof(uint32_t), 0);
        return (uint8_t *)ptr;
    }

    inline void allocate(size_t size, ConfigurationParameter &parameter)
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
        param._readable = allocate(size, nullptr);
    }

    inline void deallocate(ConfigurationParameter &parameter)
    {
        auto &param = parameter._getParam();
        if (param.isWriteable()) {
            //param._length = param._writeable->length();
            delete param._writeable;
            param._writeable = nullptr;
            param._is_writeable = false;
        }
        if (param._readable) {
            free(param._readable);
            param._readable = nullptr;
        }
        // param._usage._counter2 = param._usage._counter + 1;
    }

    inline size_type getParameterLength(ParameterType type, size_t length)
    {
        switch (type) {
        case ParameterType::BYTE:
            return sizeof(uint8_t);
        case ParameterType::WORD:
            return sizeof(uint16_t);
        case ParameterType::DWORD:
            return sizeof(uint32_t);
        case ParameterType::QWORD:
            return sizeof(uint64_t);
        case ParameterType::FLOAT:
            return sizeof(float);
        case ParameterType::DOUBLE:
            return sizeof(double);
        case ParameterType::BINARY:
        case ParameterType::STRING:
            return (size_type)length;
        default:
            break;
        }
        __DBG_panic("invalid type=%u length=%u", type, length);
        return 0;
    }

}
