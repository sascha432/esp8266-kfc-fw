/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include <type_traits>
#include <functional>
#include "type_traits.h"

namespace STL_STD_EXT_NAMESPACE {

#if __HAS_CPP17 == 0

    template <typename _Ta, typename _Tpred>
    const _Ta& clamp(const _Ta& value, const _Ta& minValue, const _Ta& maxValue, _Tpred pred) {
        if (pred(maxValue, value)) {
            return maxValue;
        }
        if (pred(value, minValue)) {
            return minValue;
        }
        return value;
    }

    template <typename _Ta>
    const _Ta& clamp(const _Ta& value, const _Ta& minValue, const _Ta& maxValue) {
        if (maxValue < value) {
            return maxValue;
        }
        if (value < minValue) {
            return minValue;
        }
        return value;
    }

#endif

}

namespace STL_STD_EXT_NAMESPACE_EX {

    template<typename _Ta, typename _Tb, typename _Tc, typename _Tpred, typename _Tret = std::common_type_t<std::make_unsigned_t<_Ta>, std::make_unsigned_t<_Tb>, std::make_unsigned_t<_Tc>>>
    _Tret clamp_unsigned(_Ta value, _Tb minValue, _Tc maxValue, _Tpred pred)
    {
        return std::clamp(static_cast<_Tret>(value), static_cast<_Tret>(minValue), static_cast<_Tret>(maxValue), pred);
    }

    template<typename _Ta, typename _Tb, typename _Tc, typename _Tret = typename std::common_type<typename std::make_unsigned<_Ta>::type, typename std::make_unsigned<_Tb>::type, typename std::make_unsigned<_Tc>::type>::type>
    _Tret clamp_unsigned(_Ta value, _Tb minValue, _Tc maxValue)
    {
        return std::clamp(static_cast<_Tret>(value), static_cast<_Tret>(minValue), static_cast<_Tret>(maxValue));
    }

    template<typename _Ta, typename _Tb, typename _Tc, typename _Tpred, typename _Tret = typename std::common_type<typename std::make_signed<_Ta>::type, typename std::make_signed<_Tb>::type, typename std::make_signed<_Tc>::type>::type>
    _Tret clamp_signed(_Ta value, _Tb minValue, _Tc maxValue, _Tpred pred)
    {
        return std::clamp(static_cast<_Tret>(value), static_cast<_Tret>(minValue), static_cast<_Tret>(maxValue), pred);
    }

    template<typename _Ta, typename _Tb, typename _Tc, typename _Tret = typename std::common_type<typename std::make_signed<_Ta>::type, typename std::make_signed<_Tb>::type, typename std::make_signed<_Tc>::type>::type>
    _Tret clamp_signed(_Ta value, _Tb minValue, _Tc maxValue)
    {
        return std::clamp(static_cast<_Tret>(value), static_cast<_Tret>(minValue), static_cast<_Tret>(maxValue));
    }

}
