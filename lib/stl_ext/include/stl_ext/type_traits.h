/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include <stdint.h>
#include <type_traits>
#include <limits>

#pragma push_macro("max")
#undef max

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


    //
    // return underlying type for enum or T if not enum
    //
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

    //
    // returns true if type is a "c" string
    // char *, char[], const char *, const char[]
    //
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

    //
    // Determine smallest unsigned integral type for maximum _Value
    // uint8_t - uint64_t
    //
    template<uint64_t _Value>
    class unsigned_integral_type_helper {
    public:
        using type = std::conditional_t<_Value <= UINT8_MAX,
            uint8_t,
            std::conditional_t<_Value <= UINT16_MAX,
                uint16_t, std::conditional_t<_Value <= UINT32_MAX,
                    uint32_t,
                    uint64_t
                >
            >
        >;
    };

    template<uint64_t _Value>
    using unsigned_integral_type_helper_t = typename unsigned_integral_type_helper<_Value>::type;

    //
    // Determine smallest unsigned type for maximum _Value
    // bool - uint64_t
    //
    template<uint64_t _Value>
    class unsigned_type_helper {
    public:
        using type = std::conditional_t<_Value <= 1, bool, unsigned_integral_type_helper_t<_Value>>;
    };

    template<uint64_t _Value>
    using unsigned_type_helper_t = typename unsigned_type_helper<_Value>::type;

    //
    // Determine smallest signed integral type for maximum _Value or minimum _Value
    // int8_t - int64_t
    //
    template<int64_t _Value>
    class signed_integral_type_helper {
    public:
        using type = std::make_signed_t<
            std::conditional_t<
                _Value >= 0,
                unsigned_integral_type_helper_t<static_cast<uint64_t>(_Value) << 1>,
                unsigned_integral_type_helper_t<(static_cast<uint64_t>(~_Value)) << 1>
            >
        >;
    };

    template<int64_t _Value>
    using signed_integral_type_helper_t = typename signed_integral_type_helper<_Value>::type;

    //
    // Determine smallest signed type or type bool for maximum _Value or minimum _Value
    // int8_t - int64_t
    //
    template<int64_t _Value>
    class signed_type_helper {
    public:
        using type = std::conditional_t<_Value == 0 || _Value == 1,
            bool,
            signed_integral_type_helper_t<_Value>
        >;
    };

    template<int64_t _Value>
    using signed_type_helper_t = typename signed_type_helper<_Value>::type;

    //
    // Determine common type for minimum and positive maximum value
    //
    // unsigned: uint8_t - uint64_t
    // signed: int8_t - int64_t
    template<int64_t _MinValue, uint64_t _MaxValue>
    class common_integral_type_helper {
    private:
        using unsigned_min_type = unsigned_integral_type_helper_t<_MinValue>;
        using unsigned_max_type = unsigned_integral_type_helper_t<_MaxValue>;
        using signed_min_type = signed_integral_type_helper_t<_MinValue>;
        using signed_max_type = signed_integral_type_helper_t<_MaxValue>;
    public:
        static constexpr bool is_min_value_signed = _MinValue <= -1;
        static constexpr bool is_max_value_signed = is_min_value_signed && _MaxValue <= -1;     
        static constexpr bool is_signed = is_min_value_signed || is_max_value_signed;

        static_assert((_MinValue >= 0) || (!(_MinValue >= 0) && _MaxValue <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())), "no common type available for signed _MinValue and uint64_t _MaxValue");
        static_assert((!is_signed && static_cast<int64_t>(_MinValue) <= static_cast<int64_t>(_MaxValue)) || (is_signed && _MinValue <= static_cast<int64_t>(_MaxValue)), "_MinValue must be smaller or equal _MaxValue");

        using common_signed_type = std::conditional_t<is_min_value_signed,
            std::conditional_t<sizeof(signed_min_type) >= sizeof(signed_max_type), signed_min_type, signed_max_type>,
            std::conditional_t<sizeof(signed_min_type) >= sizeof(signed_max_type), signed_max_type, signed_min_type>
        >;
        using common_unsigned_type = std::conditional_t<is_max_value_signed,
            std::conditional_t<sizeof(unsigned_max_type) >= sizeof(unsigned_min_type), unsigned_min_type, unsigned_max_type>,
            std::conditional_t<sizeof(unsigned_max_type) >= sizeof(unsigned_min_type), unsigned_max_type, unsigned_min_type>
            
        >;
        using type = std::conditional_t<is_signed, common_signed_type, common_unsigned_type>;
    };

    template<int64_t _MinValue, uint64_t _MaxValue>
    using common_integral_type_helper_t = typename common_integral_type_helper<_MinValue, _MaxValue>::type;

    //
    // Determine common type for minimum and positive maximum value
    //
    // _MinValue = 0 returns unsigned types only
    // _MinValue = -1 returns signed types only
    //
    // unsigned: bool, uint8_t - uint64_t
    // signed: int8_t - int64_t
    template<int64_t _MinValue, uint64_t _MaxValue>
    class common_type_helper {
    private:
        using unsigned_min_type = unsigned_type_helper_t<_MinValue>;
        using unsigned_max_type = unsigned_type_helper_t<_MaxValue>;
        using signed_min_type = signed_type_helper_t<_MinValue>;
        using signed_max_type = signed_type_helper_t<_MaxValue>;
    public:
        static constexpr bool is_min_value_signed = _MinValue <= -1;
        static constexpr bool is_max_value_signed = is_min_value_signed && _MaxValue <= -1;     
        static constexpr bool is_signed = is_min_value_signed || is_max_value_signed;

        static_assert((_MinValue >= 0) || (!(_MinValue >= 0) && _MaxValue <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())), "no common type available for signed _MinValue and uint64_t _MaxValue");
        static_assert((!is_signed && static_cast<int64_t>(_MinValue) <= static_cast<int64_t>(_MaxValue)) || (is_signed && _MinValue <= static_cast<int64_t>(_MaxValue)), "_MinValue must be smaller or equal _MaxValue");

        using common_signed_type = std::conditional_t<is_min_value_signed,
            std::conditional_t<sizeof(signed_min_type) >= sizeof(signed_max_type), signed_min_type, signed_max_type>,
            std::conditional_t<sizeof(signed_min_type) >= sizeof(signed_max_type), signed_max_type, signed_min_type>
        >;
        using common_unsigned_type = std::conditional_t<is_max_value_signed,
            std::conditional_t<sizeof(unsigned_max_type) >= sizeof(unsigned_min_type), unsigned_min_type, unsigned_max_type>,
            std::conditional_t<sizeof(unsigned_max_type) >= sizeof(unsigned_min_type), unsigned_max_type, unsigned_min_type>
            
        >;
        using type = std::conditional_t<is_signed, common_signed_type, common_unsigned_type>;
    };

    template<int64_t _MinValue, uint64_t _MaxValue>
    using common_type_helper_t = typename common_type_helper<_MinValue, _MaxValue>::type;


#if 0
    static_assert(std::is_same<unsigned_integral_type_helper_t<0>, uint8_t>::value, "test failed");
    static_assert(std::is_same<unsigned_integral_type_helper_t<1>, uint8_t>::value, "test failed");
    static_assert(std::is_same<unsigned_integral_type_helper_t<2>, uint8_t>::value, "test failed");
    static_assert(std::is_same<unsigned_integral_type_helper_t<UINT8_MAX>, uint8_t>::value, "test failed");
    static_assert(std::is_same<unsigned_integral_type_helper_t<UINT8_MAX + 1ULL>, uint16_t>::value, "test failed");
    static_assert(std::is_same<unsigned_integral_type_helper_t<UINT16_MAX>, uint16_t>::value, "test failed");
    static_assert(std::is_same<unsigned_integral_type_helper_t<UINT16_MAX + 1ULL>, uint32_t>::value, "test failed");
    static_assert(std::is_same<unsigned_integral_type_helper_t<UINT32_MAX>, uint32_t>::value, "test failed");
    static_assert(std::is_same<unsigned_integral_type_helper_t<UINT32_MAX + 1ULL>, uint64_t>::value, "test failed");

    static_assert(std::is_same<unsigned_type_helper_t<0>, bool>::value, "test failed");
    static_assert(std::is_same<unsigned_type_helper_t<1>, bool>::value, "test failed");
    static_assert(std::is_same<unsigned_type_helper_t<2>, uint8_t>::value, "test failed");
    static_assert(std::is_same<unsigned_type_helper_t<UINT8_MAX>, uint8_t>::value, "test failed");
    static_assert(std::is_same<unsigned_type_helper_t<UINT8_MAX + 1ULL>, uint16_t>::value, "test failed");
    static_assert(std::is_same<unsigned_type_helper_t<UINT16_MAX>, uint16_t>::value, "test failed");
    static_assert(std::is_same<unsigned_type_helper_t<UINT16_MAX + 1ULL>, uint32_t>::value, "test failed");
    static_assert(std::is_same<unsigned_type_helper_t<UINT32_MAX>, uint32_t>::value, "test failed");
    static_assert(std::is_same<unsigned_type_helper_t<UINT32_MAX + 1ULL>, uint64_t>::value, "test failed");

    static_assert(std::is_same<signed_integral_type_helper_t<INT64_MIN>, int64_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT32_MIN - 1LL>, int64_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT32_MIN>, int32_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT16_MIN>, int16_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT8_MIN - 1LL>, int16_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT8_MIN>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<-1>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<0>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<1>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<2>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT8_MAX>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT8_MAX + 1LL>, int16_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT16_MAX>, int16_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT16_MAX + 1LL>, int32_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT32_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT32_MAX + 1LL>, int64_t>::value, "test failed");
    static_assert(std::is_same<signed_integral_type_helper_t<INT64_MAX>, int64_t>::value, "test failed");

    static_assert(std::is_same<signed_type_helper_t<INT64_MIN>, int64_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT32_MIN - 1LL>, int64_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT32_MIN>, int32_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT16_MIN>, int16_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT8_MIN - 1LL>, int16_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT8_MIN>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<-1>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<0>, bool>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<1>, bool>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<2>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT8_MAX>, int8_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT8_MAX + 1LL>, int16_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT16_MAX>, int16_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT16_MAX + 1LL>, int32_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT32_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT32_MAX + 1LL>, int64_t>::value, "test failed");
    static_assert(std::is_same<signed_type_helper_t<INT64_MAX>, int64_t>::value, "test failed");

    static_assert(std::is_same<common_type_helper_t<INT8_MIN, INT8_MAX>, int8_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<-1, 1>, int8_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<-1, INT8_MAX + 1ULL>, int16_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<-1, UINT8_MAX>, int16_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<INT16_MIN, INT16_MAX>, int16_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<-1, INT16_MAX + 1ULL>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<INT16_MIN - 1LL, INT16_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<-1, UINT16_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<INT32_MIN, INT32_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<-1, INT32_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<INT32_MIN - 1LL, INT32_MAX>, int64_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<-1, INT64_MAX>, int64_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<INT64_MIN, INT64_MAX>, int64_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<-1, 0>, int8_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, 0>, bool>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, 1>, bool>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, INT8_MAX>, uint8_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, INT8_MAX + 1ULL>, uint8_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, UINT8_MAX>, uint8_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, UINT8_MAX + 1ULL>, uint16_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, INT16_MAX>, uint16_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, UINT16_MAX>, uint16_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, UINT16_MAX + 1ULL>, uint32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, INT32_MAX>, uint32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, INT32_MAX + 1ULL>, uint32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, UINT32_MAX>, uint32_t>::value, "test failed");
    static_assert(std::is_same<common_type_helper_t<0, UINT32_MAX + 1ULL>, uint64_t>::value, "test failed");

    static_assert(std::is_same<common_integral_type_helper_t<INT8_MIN, INT8_MAX>, int8_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<-1, 1>, int8_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<-1, INT8_MAX + 1ULL>, int16_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<-1, UINT8_MAX>, int16_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<INT16_MIN, INT16_MAX>, int16_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<-1, INT16_MAX + 1ULL>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<INT16_MIN - 1LL, INT16_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<-1, UINT16_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<INT32_MIN, INT32_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<-1, INT32_MAX>, int32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<INT32_MIN - 1LL, INT32_MAX>, int64_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<-1, INT64_MAX>, int64_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<INT64_MIN, INT64_MAX>, int64_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<-1, 0>, int8_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, 0>, uint8_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, 1>, uint8_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, INT8_MAX>, uint8_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, INT8_MAX + 1ULL>, uint8_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, UINT8_MAX>, uint8_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, UINT8_MAX + 1ULL>, uint16_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, INT16_MAX>, uint16_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, UINT16_MAX>, uint16_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, UINT16_MAX + 1ULL>, uint32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, INT32_MAX>, uint32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, INT32_MAX + 1ULL>, uint32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, UINT32_MAX>, uint32_t>::value, "test failed");
    static_assert(std::is_same<common_integral_type_helper_t<0, UINT32_MAX + 1ULL>, uint64_t>::value, "test failed");
#endif

}

#pragma pop_macro("max")
