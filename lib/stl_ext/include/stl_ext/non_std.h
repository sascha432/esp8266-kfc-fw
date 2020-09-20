/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include "iterator.h"

namespace non_std {

    // C++17 replacement for deprecated std::iterator

    template <typename _Category, typename _Ty, typename _Diff = ptrdiff_t, typename _Pointer = _Ty *, typename _Reference = _Ty &>
    struct iterator {
        using iterator_category = _Category;
        using value_type = _Ty;
        using difference_type = _Diff;
        using pointer = _Pointer;
        using reference = _Reference;
    };

    template <typename _Arg, typename _Result>
    struct unary_function {
        using argument_type = _Arg;
        using result_type = _Result;
    };

}
