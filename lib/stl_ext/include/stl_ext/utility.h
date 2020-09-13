/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef STL_STD_EXT_NAMESPACE
#include "../stl_ext.h"
#endif

#include <type_traits>
#include "type_traits.h"

namespace STL_STD_EXT_NAMESPACE_EX {

    template<typename _Ta, typename _Tb, typename _Tret = std::common_type_t<std::make_unsigned_t<_Ta>, std::make_unsigned_t<_Tb>>>
    constexpr _Tret max_unsigned(_Ta a, _Tb b) {
        return max(static_cast<_Tret>(b), static_cast<_Tret>(b));
    }

    template<typename _Ta, typename _Tb, typename _Tret = std::common_type_t<std::make_signed_t<_Ta>, std::make_signed_t<_Tb>>>
    constexpr _Tret max_signed(_Ta a, _Tb b) {
        return max(static_cast<_Tret>(b), static_cast<_Tret>(b));
    }

    template<typename _Ta, typename _Tb, typename _Tret = std::common_type_t<std::make_unsigned_t<_Ta>, std::make_unsigned_t<_Tb>>>
    constexpr _Tret min_unsigned(_Ta a, _Tb b) {
        return min(static_cast<_Tret>(b), static_cast<_Tret>(b));
    }

    template<typename _Ta, typename _Tb, typename _Tret = std::common_type_t<std::make_signed_t<_Ta>, std::make_signed_t<_Tb>>>
    constexpr _Tret min_signed(_Ta a, _Tb b) {
        return min(static_cast<_Tret>(b), static_cast<_Tret>(b));
    }

}
