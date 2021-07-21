/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include <memory>
#include "non_std.h"

namespace STL_STD_EXT_NAMESPACE_EX {

    // std::vector<std:unique_ptr<Test>> _timers;
    // Test *timer;
    // auto iterator = std::find_if(_timers.begin(), _timers.end(), stdex::compare_unique_ptr(timer));

    template <typename _Ta>
    class compare_unique_ptr_function : public non_std::unary_function<_Ta, bool>
    {
    protected:
        _Ta *_ptr;
    public:
        explicit compare_unique_ptr_function(_Ta *ptr) : _ptr(ptr) {}
        bool operator() (const std::unique_ptr<_Ta> &obj) const {
            return obj.get() == _ptr;
        }
    };

    template <typename _Ta>
    static inline compare_unique_ptr_function<_Ta> compare_unique_ptr(_Ta *ptr) {
        return compare_unique_ptr_function<_Ta>(ptr);
    }

#pragma push_macro("new")
#undef new

    template<typename _T>
    union __attribute__((aligned(4))) UninitializedClass {
        uint8_t buffer[(sizeof(_T) + 3) & ~3];
        uint32_t buffer32[(sizeof(_T) + 3) / sizeof(uint32_t)];
        _T _object;

        UninitializedClass()
#if DEBUG
            // erase memory otherwise it will be random / previous data
            : buffer32{}
#endif
        {
        }
        ~UninitializedClass() {}

        inline void init() {
            ::new(static_cast<void *>(&_object)) _T();
        }
    };

#pragma pop_macro("new")

}

#if __HAS_CPP14 == 0

namespace STL_STD_EXT_NAMESPACE {

    template<class _Ta, class... _Args>
    std::enable_if_t<!std::is_array<_Ta>::value, std::unique_ptr<_Ta>>
    make_unique(_Args&&... args)
    {
        return std::unique_ptr<_Ta>(new _Ta(std::forward<_Args>(args)...));
    }

    template<class _Ta>
    std::enable_if_t<is_unbounded_array<_Ta>::value, std::unique_ptr<_Ta>>
    make_unique(std::size_t n)
    {
        return std::unique_ptr<_Ta>(new std::remove_extent_t<_Ta>[n]());
    }

    template<class _Ta, class... _Args>
    std::enable_if_t<is_bounded_array<_Ta>::value> make_unique(_Args&&...) = delete;

}

#endif
