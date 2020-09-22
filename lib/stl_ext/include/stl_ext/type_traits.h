/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include <type_traits>

namespace STL_STD_EXT_NAMESPACE {

// #if __GNUG__ && __GNUC__ < 5

//     template<typename _Ta>
//     struct is_trivially_copyable : integral_constant<bool, __has_trivial_copy(_Ta)> {
//     };


// #endif

#if __HAS_CPP14 == 0

    template<typename _Ta>
    using make_unsigned_t = typename std::make_unsigned<_Ta>::type;

    template<typename _Ta>
    using make_signed_t = typename std::make_signed<_Ta>::type;

    template <class... _Ta>
    using common_type_t = typename std::common_type<_Ta...>::type;

    template <bool _Ta, class _Tb = void>
    using enable_if_t = typename std::enable_if<_Ta, _Tb>::type;

    template <bool _Test, class _Ta, class _Tb>
    using conditional_t = typename conditional<_Test, _Ta, _Tb>::type;

#ifndef _MSC_VER
    template <class _Ta>
    using add_pointer_t = typename add_pointer<_Ta>::type;
#endif

    template <class _Ta>
    using remove_const_t = typename remove_const<_Ta>::type;

    template <class _Ta>
    using remove_cv_t = typename remove_cv<_Ta>::type;

    template <class _Ta>
    using remove_pointer_t = typename remove_pointer<_Ta>::type;

    template <class _Ta>
    using remove_reference_t = typename remove_reference<_Ta>::type;

    template <class _Ta>
    using remove_extent_t = typename remove_extent<_Ta>::type;


#endif

#if __HAS_CPP17 == 0

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

    // returns true if type is a "c" string
    // char *, char[], const char *, const char[]
    template<typename _Ta>
    struct is_c_str : std::integral_constant<bool,
        std::is_same<char *,
            std::conditional_t<
                std::is_pointer<_Ta>::value || std::is_reference<_Ta>::value || std::is_array<_Ta>::value,
                std::add_pointer_t<
                    std::remove_cv_t<
                        std::remove_extent_t<
                            std::remove_pointer_t<
                                std::remove_reference_t<_Ta>
                            >
                        >
                    >
                >,
                _Ta
            >
        >::value
    >
    {
    };

}
