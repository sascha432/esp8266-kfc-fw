/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef STL_STD_EXT_NAMESPACE
#include "../stl_ext.h"
#endif

namespace STL_STD_EXT_NAMESPACE {

#if __GNUG__ && __GNUC__ < 5

    template<typename _Ta>
    struct is_trivially_copyable : integral_constant<bool, __has_trivial_copy(_Ta)> {
    };

#endif

#if !__HAS_CPP14

    template <class _Ta>
    using is_trivially_copyable_t = typename is_trivially_copyable<_Ta>::type;

#endif

}
