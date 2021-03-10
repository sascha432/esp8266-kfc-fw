/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include "type_traits.h"
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

    // copy initializer list into target without checking target size

    // uint8_t test2[4];
    // uint8_t *test = &test2[0];

    // std::copy_array<uint8_t>({ 1, 2, 3, 4 }, test2);
    // std::copy_array<uint8_t>({ 1, 2, 3, 4 }, test);

    template<typename _Ta>
    struct copy_array {
        using element_type = std::remove_pointer_t<std::remove_extent_t<_Ta>>;
        copy_array(std::initializer_list<const element_type> l, element_type *target) {
            std::copy(l.begin(), l.end(), target);
        }
    };

}
