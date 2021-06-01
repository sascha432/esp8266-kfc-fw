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


    // constexpr std::min/max

    template<typename _Ta>
    constexpr const _Ta &min(const _Ta &a, const _Ta &b)
    {
        return (a < b) ? a : b;
    }

    template<typename _Ta>
    constexpr const _Ta &min(const _Ta &a, const _Ta &b, const _Ta &c)
    {
        return min<_Ta>(min<_Ta>(a, b), c);
    }

    template<typename _Ta>
    constexpr const _Ta &min(const _Ta &a, const _Ta &b, const _Ta &c, const _Ta &d)
    {
        return min<_Ta>(min<_Ta>(min<_Ta>(a, b), c), d);
    }

    template<typename _Ta>
    constexpr const _Ta &max(const _Ta &a, const _Ta &b)
    {
        return (a < b) ? b : a;
    }

    template<typename _Ta>
    constexpr const _Ta &max(const _Ta &a, const _Ta &b, const _Ta &c)
    {
        return max<_Ta>(max<_Ta>(a, b), c);
    }

    template<typename _Ta>
    constexpr const _Ta &max(const _Ta &a, const _Ta &b, const _Ta &c, const _Ta &d)
    {
        return max<_Ta>(max<_Ta>(max<_Ta>(a, b), c), d);
    }

    namespace initializer_list {

        template<typename _Ta>
        constexpr const size_t distance(const typename std::initializer_list<_Ta>::const_iterator begin, const typename std::initializer_list<_Ta>::const_iterator end, const size_t size = 0)
        {
            return begin == end ? size : distance<_Ta>(begin + 1, end, size + 1);
        }

        template<typename _Ta >
        constexpr const _Ta min_iterator(const typename std::initializer_list<_Ta>::const_iterator begin, const typename std::initializer_list<_Ta>::const_iterator end, const _Ta __min)
        {
            return distance<_Ta>(begin, end) < 1 ?
                __min : distance<_Ta>(begin, end) < 2 ?
                    min<_Ta>(*begin, __min) : distance<_Ta>(begin, end) < 3 ?
                        min<_Ta>(*begin, *(begin + 1), __min) : min_iterator<_Ta>(begin + 1, end, min<_Ta>(*begin, *(begin + 1), __min));
        }

        template<typename _Ta >
        constexpr const _Ta max_iterator(const typename std::initializer_list<_Ta>::const_iterator begin, const typename std::initializer_list<_Ta>::const_iterator end, const _Ta __max)
        {
            return distance<_Ta>(begin, end) < 1 ?
                __max : distance<_Ta>(begin, end) < 2 ?
                    max<_Ta>(*begin, __max) : distance<_Ta>(begin, end) < 3 ?
                        max<_Ta>(*begin, *(begin + 1), __max) : max_iterator<_Ta>(begin + 1, end, max<_Ta>(*begin, *(begin + 1), __max));
        }

    }

    template<typename _Ta >
    constexpr const _Ta min(const std::initializer_list<_Ta> ilist)
    {
        return ilist.size() == 0 ?
            std::numeric_limits<_Ta>::max() : ilist.size() == 1 ?
                *ilist.begin() : ilist.size() == 2 ?
                    min<_Ta>(*ilist.begin(), *(ilist.begin() + 1)) : initializer_list::min_iterator<_Ta>(ilist.begin() + 1, ilist.end(), min<_Ta>(*ilist.begin(), *(ilist.begin() + 1)));
    }

    template<typename _Ta >
    constexpr const _Ta max(const std::initializer_list<_Ta> ilist)
    {
        return ilist.size() == 0 ?
            std::numeric_limits<_Ta>::min() : ilist.size() == 1 ?
                *ilist.begin() : ilist.size() == 2 ?
                    max<_Ta>(*ilist.begin(), *(ilist.begin() + 1)) : initializer_list::max_iterator<_Ta>(ilist.begin() + 1, ilist.end(), max<_Ta>(*ilist.begin(), *(ilist.begin() + 1)));
    }


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
