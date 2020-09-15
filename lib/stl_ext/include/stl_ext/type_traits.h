/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef STL_STD_EXT_NAMESPACE
#include "../stl_ext.h"
#endif

#include <type_traits>

namespace STL_STD_EXT_NAMESPACE {

// #if __GNUG__ && __GNUC__ < 5

//     template<typename _Ta>
//     struct is_trivially_copyable : integral_constant<bool, __has_trivial_copy(_Ta)> {
//     };


// #endif

#if !__HAS_CPP14

    template<typename _Ta>
    using make_unsigned_t = typename std::make_unsigned<_Ta>::type;

    template<typename _Ta>
    using make_signed_t = typename std::make_signed<_Ta>::type;

    template <class... _Ta>
    using common_type_t = typename std::common_type<_Ta...>::type;

    template <bool _Ta, class _Tb = void>
    using enable_if_t = typename std::enable_if<_Ta, _Tb>::type;

#endif

#if !__HAS_CPP17

    // template<typename _Ta>
    // inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<_Ta>::value;

#endif

}

namespace STL_STD_EXT_NAMESPACE_EX {

    template<typename _Ta>
    struct member_pointer_class;

    template<typename Class, typename Value>
    struct member_pointer_class<Value Class:: *>
    {
        typedef Class type;
    };

    template<typename _Ta>
    using member_pointer_class_t = typename member_pointer_class<_Ta>::type;

    template<typename _Ta>
    struct member_pointer_value;

    template<typename Class, typename Value>
    struct member_pointer_value<Value Class:: *>
    {
        typedef Value type;
    };

    template<typename _Ta>
    using member_pointer_value_t = typename member_pointer_value<_Ta>::type;



    template <typename T, bool = std::is_enum<T>::value>
    struct relaxed_underlying_type {
        using type = typename std::underlying_type<T>::type;
    };

    template <typename T>
    struct relaxed_underlying_type<T, false> {
        using type = T;
    };

    template <class _Ta>
    using relaxed_underlying_type_t = typename relaxed_underlying_type<_Ta>::type;

}
