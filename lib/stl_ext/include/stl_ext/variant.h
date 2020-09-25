/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include <algorithm>

#if __HAS_CPP17 == 0

#if _CRTDBG_MAP_ALLOC || defined(new)
#pragma push_macro("new")
#undef new
#endif

// TESTING this promising C++11 version variant

#include "mpark/variant.hpp"

namespace STL_STD_EXT_NAMESPACE_EX {

    using bad_variant_access = mpark::bad_variant_access;
    
    template <typename... Ts>
    using variant = mpark::variant<Ts...>;

    template <typename T>
    using variant_size = mpark::variant_size<T>;

    template <std::size_t I, typename T>
    using variant_alternative = mpark::variant_alternative<I, T>;

    template <std::size_t I, typename T>
    using variant_alternative_t = typename mpark::variant_alternative<I, T>::type;

    constexpr std::size_t variant_npos = static_cast<std::size_t>(-1);

}

#if _CRTDBG_MAP_ALLOC
#pragma pop_macro("new")
#endif

#endif
