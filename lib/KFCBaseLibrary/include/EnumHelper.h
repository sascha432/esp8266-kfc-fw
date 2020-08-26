/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <type_traits>

namespace EnumHelper {

     template <typename T>
     static constexpr int getInt(T value) {
         return static_cast<int>(value);
     }

    class Bitset {
    public:
        template <typename Tn>
        static constexpr Tn all(Tn first) {
            return first;
        }

        // add or remove bits
        template<typename Ta, typename... Args>
        static Ta getSetBits(Ta value, bool state, Args... args) {
            if (state)  {
                return addBits(value, args...);
            }
            else {
                return removeBits(value, args...);
            }
        }

        // add or remove bits
        template<typename Ta, typename... Args>
        static void setBits(Ta &value, bool state, Args... args) {
            if (state)  {
                value = addBits(value, args...);
            }
            else {
                value = removeBits(value, args...);
            }
        }

        template<typename Ta, typename... Args>
        static constexpr Ta addBits(Ta value, Args... args) {
            return getOr(value, all(args...));
        }

        template<typename Ta, typename... Args>
        static constexpr Ta removeBits(Ta value, Args... args) {
            return getAnd(value, getInverted(args...));
        }

        // AND for any type
        template<typename Tn, typename... Args>
        static constexpr bool hasAny(Tn value, Args... args) {
            return static_cast<typename std::underlying_type<Tn>::type>(value) & static_cast<typename std::underlying_type<Tn>::type>(all(args...));
        }

        template<typename Tn, typename... Args>
        static constexpr bool hasAll(Tn value, Args... args) {
            return (static_cast<typename std::underlying_type<Tn>::type>(value) & static_cast<typename std::underlying_type<Tn>::type>(all(args...))) == static_cast<typename std::underlying_type<Tn>::type>(all(args...));
        }

        template<typename Tn, typename... Args>
        static constexpr bool has(Tn value, Args... args) {
            return hasAll(value, args...);
        }

        // OR for any type
        template<typename Tn, typename... Args>
        static constexpr Tn all(Tn first, Args... args) {
            return static_cast<Tn>(static_cast<typename std::underlying_type<Tn>::type>(first) | static_cast<typename std::underlying_type<Tn>::type>(all(args...)));
        }

        template<typename T1, typename Tr = T1>
        static constexpr Tr getInverted(T1 value) {
            return static_cast<Tr>(~static_cast<typename std::underlying_type<T1>::type>(value));
        }

        // AND for 2 values of any type return Type T1
        template<typename T1, typename T2, typename Tr = T1, typename T1u = typename std::underlying_type<T1>::type, typename T2u = typename std::underlying_type<T2>::type>
        static constexpr Tr getAnd(T1 value1, T2 value2) {
            return static_cast<Tr>(static_cast<T1u>(value1) & static_cast<T2u>(value2));
        }

        // OR for 2 values of any type return Type T1
        template<typename T1, typename T2, typename Tr = T1, typename T1u = typename std::underlying_type<T1>::type, typename T2u = typename std::underlying_type<T2>::type>
        static constexpr Tr getOr(T1 value1, T2 value2) {
            return static_cast<Tr>(static_cast<T1u>(value1) | static_cast<T2u>(value2));
        }
    };

    template<typename Te>
    class BitsetT : public Bitset {
    public:
        using T = Te;
        using Type = T;
        using BaseType = typename std::underlying_type<T>::type;

        // AND for 2 values of any type return Type T
        template<typename T1, typename T2, typename Tr = T, typename T1u = typename std::underlying_type<T1>::type, typename T2u = typename std::underlying_type<T2>::type>
        static constexpr Tr getAnd(T1 value1, T2 value2) {
            return static_cast<Tr>(static_cast<T1u>(value1) & static_cast<T2u>(value2));
        }

        // OR for 2 values of any type return Type T1
        template<typename T1, typename T2, typename Tr = T, typename T1u = typename std::underlying_type<T1>::type, typename T2u = typename std::underlying_type<T2>::type>
        static constexpr Tr getOr(T1 value1, T2 value2) {
            return static_cast<Tr>(static_cast<T1u>(value1) | static_cast<T2u>(value2));
        }
    };

};

