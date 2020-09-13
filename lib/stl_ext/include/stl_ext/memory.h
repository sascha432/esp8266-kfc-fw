/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef STL_STD_EXT_NAMESPACE
#include "../stl_ext.h"
#endif

#include <memory>

namespace STL_STD_EXT_NAMESPACE_EX {

    // std::vector<std:unique_ptr<Test>> _timers;
    // Test *timer;
    // auto iterator = std::find_if(_timers.begin(), _timers.end(), std::compare_unique_ptr(timer));

    template <class _Ta>
    class compare_unique_ptr_function : public std::unary_function<_Ta, bool>
    {
    protected:
        _Ta *_ptr;
    public:
        explicit compare_unique_ptr_function(_Ta *ptr) : _ptr(ptr) {}
        bool operator() (const std::unique_ptr<_Ta> &obj) const {
            return obj.get() == _ptr;
        }
    };

    template <class _Ta>
    static inline compare_unique_ptr_function<_Ta> compare_unique_ptr(_Ta *ptr) {
        return compare_unique_ptr_function<_Ta>(ptr);
    }

}
