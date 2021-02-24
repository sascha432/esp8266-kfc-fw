/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include <type_traits>
#include <stdlib.h>
#include "type_traits.h"

namespace STL_STD_EXT_NAMESPACE {

#if __HAVE_CPP14 == 0 && _MSC_VER == 0

    template<class T, class U = T>
    T exchange(T& obj, U&& new_value)
    {
        T old_value = move(obj);
        obj = forward<U>(new_value);
        return old_value;
    }

#endif

}
namespace STL_STD_EXT_NAMESPACE_EX {

    inline static int randint(int from, int to) {
        return (rand() % (to - from)) - from;
    }

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

    // creates an object on the stack and returns a reference to it
    // the object gets destroyed when the function returns
    //
    //  void myFunc(String &str) {
    //      str += "456";
    //      Serial.println(str);
    //  }
    //
    //  myFunc(&std::stack_reference<String>(123));
    //
    // equal to
    //
    //  {
    //      String tmp(123);
    //      myFunc(tmp);
    //  }

    template<typename _Ta>
    class stack_reference {
    public:
        template<typename... Args>
        stack_reference(Args &&... args) : _object(std::forward<Args>(args)...) {
        }
        _Ta &get() {
            return _object;
        }
        const _Ta &get() const {
            return _object;
        }
        _Ta &operator&() {
            return _object;
        }
        const _Ta &operator&() const {
            return _object;
        }
    private:
        _Ta _object;
    };

}

