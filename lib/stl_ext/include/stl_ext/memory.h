/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include <memory>
#include "./non_std.h"

#ifndef DEBUG_UNINITIALIZEDCLASSES
#   if DEBUG
#       define DEBUG_UNINITIALIZEDCLASSES 1
#   else
#       define DEBUG_UNINITIALIZEDCLASSES 0
#   endif
#endif

#if DEBUG_UNINITIALIZEDCLASSES

    #include <boost/preprocessor/stringize.hpp>
    #include <boost/preprocessor/cat.hpp>
    #include <boost/preprocessor/comma.hpp>
    #include <boost/preprocessor/comma_if.hpp>
    #include <boost/preprocessor/dec.hpp>
    #include <boost/preprocessor/repetition/repeat.hpp>
    #include <boost/preprocessor/variadic/size.hpp>
    #include <boost/preprocessor/variadic/to_tuple.hpp>
    #include <boost/preprocessor/tuple/elem.hpp>
    #include <boost/preprocessor/expand.hpp>
    #include <boost/preprocessor/punctuation/remove_parens.hpp>
    #include <boost/preprocessor/control/if.hpp>
    #include <boost/preprocessor/comparison/equal.hpp>

    #define _UNINITIALIZEDCLASS_PAIR(var) stdex::UninitializedClassPair(var,BOOST_PP_STRINGIZE(var))

    #define _UNINITIALIZEDCLASS_CLEAR_ALL_TYPE(z, n, var)  BOOST_PP_REMOVE_PARENS(BOOST_PP_IF(BOOST_PP_EQUAL(n,0),(decltype(_UNINITIALIZEDCLASS_PAIR(BOOST_PP_TUPLE_ELEM(n,var)))),(,decltype(_UNINITIALIZEDCLASS_PAIR(BOOST_PP_TUPLE_ELEM(n,var))))))
    #define _UNINITIALIZEDCLASS_CLEAR_ALL_PAIR(z, n, var)  BOOST_PP_REMOVE_PARENS(BOOST_PP_IF(BOOST_PP_EQUAL(n,0),(_UNINITIALIZEDCLASS_PAIR(BOOST_PP_TUPLE_ELEM(n,var))),(,_UNINITIALIZEDCLASS_PAIR(BOOST_PP_TUPLE_ELEM(n,var)))))

    #define _UNINITIALIZEDCLASS_CLEAR_ALL_TYPES(...) BOOST_PP_REPEAT(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),_UNINITIALIZEDCLASS_CLEAR_ALL_TYPE,BOOST_PP_VARIADIC_TO_TUPLE(__VA_ARGS__))
    #define _UNINITIALIZEDCLASS_CLEAR_ALL_PAIRS(...) BOOST_PP_REPEAT(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),_UNINITIALIZEDCLASS_CLEAR_ALL_PAIR,BOOST_PP_VARIADIC_TO_TUPLE(__VA_ARGS__))

    #define _UNINITIALIZEDCLASS_CLEAR_ALL(...) stdex::UninitializedClass<int>::clearAll<_UNINITIALIZEDCLASS_CLEAR_ALL_TYPES(__VA_ARGS__)>(_UNINITIALIZEDCLASS_CLEAR_ALL_PAIRS(__VA_ARGS__))

    #define _UNINITIALIZEDCLASS_NAME(ns) BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(ns,_DebugClear_),__LINE__),_),__COUNTER__),__COUNTER__),__COUNTER__)

    #define _UNINITIALIZEDCLASS_DEBUG_CLEAR(name, ...) extern "C" uint32_t &name; uint32_t &name = _UNINITIALIZEDCLASS_CLEAR_ALL(__VA_ARGS__);

    #define UNINITIALIZEDCLASS_DEBUG_CLEAR(...) _UNINITIALIZEDCLASS_DEBUG_CLEAR(_UNINITIALIZEDCLASS_NAME(UninitializedClass),__VA_ARGS__)
    #define UNINITIALIZEDCLASS_DEBUG_CLEAR_NS(ns, ...) _UNINITIALIZEDCLASS_DEBUG_CLEAR(_UNINITIALIZEDCLASS_NAME(ns),__VA_ARGS__)


#else

    #define UNINITIALIZEDCLASS_DEBUG_CLEAR(...)
    #define UNINITIALIZEDCLASS_DEBUG_CLEAR_NS(...)

#endif

namespace STL_STD_EXT_NAMESPACE_EX {

    // std::vector<std:unique_ptr<Test>> _timers;
    // Test *timer;
    // auto iterator = std::find_if(_timers.begin(), _timers.end(), stdex::compare_unique_ptr(timer));

    template <typename _Ta>
    class compare_unique_ptr_function : public non_std::unary_function<_Ta, bool>
    {
    protected:
        _Ta *_ptr;
    public:
        explicit compare_unique_ptr_function(_Ta *ptr) : _ptr(ptr) {}
        bool operator() (const std::unique_ptr<_Ta> &obj) const {
            return obj.get() == _ptr;
        }
    };

    template <typename _Ta>
    static inline compare_unique_ptr_function<_Ta> compare_unique_ptr(_Ta *ptr) {
        return compare_unique_ptr_function<_Ta>(ptr);
    }

    #if DEBUG_UNINITIALIZEDCLASSES
        extern "C" uint32_t uint32_nullptr;
        inline uint32_t uint32_nullptr = 0;

        template<typename _T>
        union __attribute__((aligned(4))) UninitializedClass;

        template<typename _Ta>
        struct UninitializedClassPair {
            using value_type = UninitializedClass<_Ta>;
            using reference = value_type &;

            const value_type *_object;
            PGM_P _name;

            UninitializedClassPair(const value_type &object, PGM_P name);

            inline __attribute__((__always_inline__)) value_type *object() const {
                return const_cast<value_type *>(_object);
            }

            inline __attribute__((__always_inline__)) PGM_P name() const {
                return _name;
            }

            constexpr size_t size() const {
                return sizeof(value_type);
            }
        };

    #endif

    template<typename _T>
    union __attribute__((aligned(4))) UninitializedClass {
        using value_type = _T;

        static constexpr size_t kDWordSize = ((sizeof(value_type) + 3) >> 2);
        static constexpr size_t kPaddedSize = kDWordSize << 2;

        uint32_t _buffer[kDWordSize];
        value_type _object;

        // the constructor will be called after init()
        UninitializedClass() {}
        ~UninitializedClass() {}

   #if DEBUG_UNINITIALIZEDCLASSES

        static inline __attribute__((__always_inline__)) uint32_t &clearAll() {
            return uint32_nullptr;
        }

        template<typename _Ta>
        static inline __attribute__((__always_inline__)) uint32_t &clearAll(_Ta arg) {
            ::printf(PSTR("UninitializedClass::clear() name=%s object=%p size=%u\n"), arg.name(), arg.object(), arg.size());
            arg.object()->clear();
            return uint32_nullptr;
        }

        // clear memory of all objects passed for debugging
        //
        // static stdex::UninitializedClass<std::array<uint8_t, 10>> pinsNoInit __attribute__((section(".noinit")));
        // static stdex::UninitializedClass<std::array<uint32_t, 3>> statesNoInit __attribute__((section(".noinit")));
        // UNINITIALIZEDCLASS_DEBUG_CLEAR(pinsNoInit, statesNoInit);
        // decltype(pinsNoInit._object) &_pins = pinsNoInit;
        // decltype(statesNoInit._object) &_states = statesNoInit;

        template<typename _Ta, typename ...Args>
        static inline __attribute__((__always_inline__)) uint32_t &clearAll(_Ta arg, const Args ...args) {
            clearAll<decltype(arg)>(arg);
            return clearAll(args...);
        }

        inline __attribute__((__always_inline__)) void clear() {
            std::fill(std::begin(_buffer), std::end(_buffer), 0);
        }

        #endif

#pragma push_macro("new")
#undef new

        // init() must be called for each uninitialized object
        inline __attribute__((__always_inline__)) void init() {
            ::new(static_cast<void *>(&_object)) _T();
        }

#pragma pop_macro("new")

    };

    #if DEBUG_UNINITIALIZEDCLASSES

        template<typename _Ta>
        inline UninitializedClassPair<_Ta>::UninitializedClassPair(const value_type &object, const char name[]) : _object(&object), _name(&name[0]) {}

    #endif

}

#if __HAS_CPP14 == 0

namespace STL_STD_EXT_NAMESPACE {

    template<class _Ta, class... _Args>
    std::enable_if_t<!std::is_array<_Ta>::value, std::unique_ptr<_Ta>>
    make_unique(_Args&&... args)
    {
        return std::unique_ptr<_Ta>(new _Ta(std::forward<_Args>(args)...));
    }

    template<class _Ta>
    std::enable_if_t<is_unbounded_array<_Ta>::value, std::unique_ptr<_Ta>>
    make_unique(std::size_t n)
    {
        return std::unique_ptr<_Ta>(new std::remove_extent_t<_Ta>[n]());
    }

    template<class _Ta, class... _Args>
    std::enable_if_t<is_bounded_array<_Ta>::value> make_unique(_Args&&...) = delete;

}

#endif
