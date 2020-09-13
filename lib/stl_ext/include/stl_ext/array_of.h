/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef STL_STD_EXT_NAMESPACE
#include "../stl_ext.h"
#endif

#include <array>

namespace STL_STD_EXT_NAMESPACE {

    template <typename _Ta, typename... _Tb>
    constexpr auto array_of(_Tb&&... t) -> std::array <_Ta, sizeof...(_Tb)> {
        return {
            {
                (_Ta)std::forward<_Tb>(t)...
            }
        };
    }

}
