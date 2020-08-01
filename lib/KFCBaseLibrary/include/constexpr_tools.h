/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>

#pragma once

namespace ArrayConstExpr {

    template <std::size_t ...>
    struct range
    { };

    template <std::size_t N, std::size_t ... Next>
    struct rangeH
    {
        using type = typename rangeH<N - 1U, N - 1U, Next ... >::type;
    };

    template <std::size_t ... Next >
    struct rangeH<0U, Next ... >
    {
        using type = range<Next ... >;
    };

    template <class ElementType, std::size_t len>
    class ArrayAccess {
    public:
        ElementType *elements;

        constexpr ArrayAccess(ElementType *elementsPtr) : elements(elementsPtr)
        {
        }

        constexpr ElementType operator[](size_t pos)
        {
            return pos >= len ? 0 : elements[pos];
        }
    };

    template <class AccessType, class ElementType, std::size_t len>
    class ArrayHelper
    {
    public:
        ElementType array[len + 1];

        template <std::size_t ... rng>
        constexpr ArrayHelper(AccessType arr, const range<rng...>&)
            : array{ arr[rng]... }
        { }

        constexpr ArrayHelper(AccessType arr)
            : ArrayHelper(arr, typename rangeH<len + 1>::type())
        { }
    };


};


namespace StringConstExpr {

    using ArrayConstExpr::ArrayAccess;
    using ArrayConstExpr::ArrayHelper;

    template <std::size_t len>
    class StringArrayHelper : public ArrayHelper<ArrayAccess<const char, len>, const char, len>
    {
    public:
        using ArrayHelper<ArrayAccess<const char, len>, const char, len>::ArrayHelper;
        using ArrayHelper<ArrayAccess<const char, len>, const char, len>::ArrayHelper::array;

        operator const __FlashStringHelper* () const {
            return reinterpret_cast<const __FlashStringHelper*>(&array[0]);
        }
        operator const char *() const {
            return reinterpret_cast<const char *>(&array[0]);
        }
    };

    template<std::size_t index, std::size_t len>
    class SubStringArray : public StringArrayHelper<len> {
    public:
        constexpr SubStringArray(const char* str) : StringArrayHelper<len>(ArrayAccess<const char, len>(str + index).elements) {
        }
    };

    template<std::size_t len>
    class StringArray : public SubStringArray<0, len> {
    public:
        StringArray(const char* str) : SubStringArray<0, len>(ArrayAccess<const char, len>(str).elements) {
        }
    };

    //static const StringConstExpr::SubStringArray<4, 3> progmem_str PROGMEM("longstring");
    //static const StringConstExpr::StringArray<4> progmem_long PROGMEM("longstring");
    //Serial.println(progmem_str); // output "str"
    //Serial.println(progmem_long); // output "long"
    //Serial.println(FPSTR(progmem_str.array));

    namespace SlowStrFuncs {

        // returns end of string either when found NUL or when limit characters have been counted
        constexpr int strlen(const char* str, int limit = -1, int pos = -1) {
            return str ? (pos == -1 ? strlen(str, limit, 0) : (limit != 0 && *(str + pos) ? strlen(str, limit - 1, pos + 1) : pos)) : 0;
        }

        constexpr int strlen(const uint8_t *str, int limit = -1, int pos = -1) {
            return str ? (pos == -1 ? strlen(str, limit, 0) : (limit != 0 && *(str + pos) ? strlen(str, limit - 1, pos + 1) : pos)) : 0;
        }

        // return first occurrence of ch in str before limit characters
        constexpr const char* strchr(const char* str, int ch, int limit = -1, const char* ptr = nullptr) {
            return str ? (ptr ? ((ptr < str + strlen(str, limit) ? (*ptr == ch ? ptr : strchr(str, ch, limit, ptr + 1)) : nullptr)) : strchr(str, ch, limit, str)) : nullptr;
        }

        // returns last occurrence of ch in str starting from limit characters or the end of the string
        constexpr const char* strrchr(const char* str, int ch, int limit = -1, const char* ptr = nullptr) {
            return str ? (ptr ? ((ptr >= str ? (*ptr == ch ? ptr : (ptr == str ? nullptr : strrchr(str, ch, limit, ptr - 1))) : nullptr)) : strrchr(str, ch, limit, str + strlen(str, limit) - 1)) : nullptr;
        }

        constexpr bool strings_equal(char const *a, char const *b) {
            return (uint8_t)*a == (uint8_t)*b && (*a == 0 || strings_equal(a + 1, b + 1));
        }

        constexpr bool strncmp(char const *a, char const *b, int n) {
            return (n == 0) || (((uint8_t)*a == (uint8_t)*b) && (*a == 0 || SlowStrFuncs::strncmp(a + 1, b + 1, n - 1)));
        }

    };

#if __GNUC__
    constexpr int strlen(const char* str, int limit = -1, int pos = -1) {
        return (limit == -1 && pos == - 1) ? __builtin_strlen(str) : SlowStrFuncs::strlen(str, limit, pos);
    }
    constexpr int strlen(const uint8_t* str) {
        return SlowStrFuncs::strlen(str);
    }
    constexpr const char* strchr(const char* str, int ch, int limit = -1, const char* ptr = nullptr) {
        return limit == -1 ? __builtin_strchr(str, ch) : SlowStrFuncs::strchr(str, ch, limit, ptr);
    }
    constexpr const char* strrchr(const char* str, int ch, int limit = -1, const char* ptr = nullptr) {
        return limit == -1 ? __builtin_strrchr(str, ch) : SlowStrFuncs::strrchr(str, ch, limit, ptr);
    }
    // constexpr int strcmp(const char *str1, const char *str2) {
    //     return __builtin_strcmp(str1, str2);
    // }
#else
    using SlowStrFuncs::strlen;
    using SlowStrFuncs::strchr;
    using SlowStrFuncs::strrchr;
    using SlowStrFuncs::strncmp;
#endif
    constexpr int strcmp(const char *str1, const char *str2) {
        return SlowStrFuncs::strings_equal(str1, str2) == true ? 1 : 0;
    }

    // returns strlen() if endptr is null or the start_length of the string with endptr being the NUL character
    constexpr int strlen_endptr(const char* str, const char* ptr = nullptr) {
        return ptr ? strlen(str, (int)(ptr - str)) : strlen(str);
    }

    // bascially (end - start) for different pointers with the same content and it returns -1 if start or end is null
    constexpr int strlen_diff(const char* start, const char* end, int limit = -1) {
        return start && end ? strlen(start, limit) - strlen(end, limit) : -1;
    }

    // returns any occurrence of str2 in str1 before limit characters
    constexpr const char* strpbrk(const char* str1, const char* str2, int limit = -1, const char* ptr = nullptr) {
        return (str1 && str2) ? (ptr ? ((ptr < str1 + strlen(str1, limit) ? (strchr(str2, *ptr) ? ptr : strpbrk(str1, str2, limit, ptr + 1)) : nullptr)) : strpbrk(str1, str2, limit, str1)) : nullptr;
    }

    constexpr const char *basename(const char* filename) {
        return !strrchr(filename, '/') ? (!strrchr(filename, '\\') ? filename : strrchr(filename, '\\') + 1) : strrchr(filename, '/') + 1;
    }

    // convert binary, octal, decimal and hex digits to int, returns 255 if invalid charaters
    constexpr int convert_digit(char ch) {
        return isdigit(ch) ? ch - 48 : ch == 'a' ? 10 : ch == 'b' ? 11 : ch == 'c' ? 12 : ch == 'd' ? 13 : ch == 'e' ? 14 : ch == 'f' ? 15 : ch == 'A' ? 10 : ch == 'B' ? 11 : ch == 'C' ? 12 : ch == 'D' ? 13 : ch == 'E' ? 14 : ch == 'F' ? 15 : 255;
    }

    // use convert_digit to convert the digit and validate  with given base
    constexpr int isdigit_base(const char* str, int base = 10) {
        return convert_digit(*str) < base ? convert_digit(*str) : 255;
    }

    constexpr int strtoll_base(const char* str) {
        return *str == '0' ? (str[1] == 'x' ? 16 : str[1] == 'b' ? 2 : 8) : 10;
    }

    constexpr const char* strtoll_start(const char* str) {
        return *str == '0' ? (str[1] == 'x' || str[1] == 'b') ? str + 2 : str : str;
    }

    constexpr int64_t strtoll(const char* str, int64_t value = 0, int base = 0, int pos = 0, int64_t invalid_value = (int64_t)-1) {
        return ((base == 0) ? (strtoll(strtoll_start(str), value, strtoll_base(str), 0, invalid_value)) : (
            (pos == 0 && *str == '-') ? (-strtoll(str + 1, value, base, 0, invalid_value)) :
                (isdigit_base(str, base) != 255 ? strtoll(str + 1, (value * base) + isdigit_base(str, base), base, pos + 1, invalid_value) : pos == 0 ? invalid_value : value)));
    }

};

namespace DebugHelperConstExpr {

    constexpr const char* get_class_name_end(const char* str) {
        return StringConstExpr::strchr(str, '<') ? StringConstExpr::strchr(str, '<') : StringConstExpr::strchr(str, ':');
    }

    constexpr const char* get_class_name_start(const char* str) {
        return StringConstExpr::strrchr(str, ' ', StringConstExpr::strlen(get_class_name_end(str))) ? StringConstExpr::strrchr(str, ' ', StringConstExpr::strlen(get_class_name_end(str))) + 1 : str;
    }

    constexpr std::size_t get_class_name_len(const char* str, const char *end = nullptr) {
        return StringConstExpr::strlen(get_class_name_start(str), StringConstExpr::strlen_diff(get_class_name_start(str), get_class_name_end(str)));
    }


    //constexpr const char* trim_pretty_function(const char* str, const char *ptr = nullptr) {
    //    return (ptr == nullptr) ? (strchr(str, '(') ? trim_pretty_function(str, strchr(str, '(')) : str) : (*ptr == ' ' ? (ptr + 1) : (ptr > str ? trim_pretty_function(str, ptr - 1) : str));
    //}

};

namespace EnumBaseConstExpr {

    using StringConstExpr::strchr;

    constexpr const char* trim(const char* str) {
        return strchr(" \t\r\n,=", *str) ? trim(str + 1) : str;
    }

    constexpr const char* end(const char* str) {
        return !*str || strchr("\t\r\n,=", *str) ? str : end(str + 1);
    }

    constexpr int len(const char* str) {
        return str && end(str) ? end(str) - str : 0;
    }

    constexpr const char* next(const char* str, int num = 0) {
        return num == 0 ? !*str ? nullptr : str : strchr(str, ',') ? next(strchr(str, ',') + 1, num - 1) : nullptr;
    }

    constexpr const char* name(const char* str, int num = 0) {
        return !str ? nullptr : num != 0 ? name(next(str, num)) : (len(trim(str)) ? trim(str) : nullptr);
    }

    constexpr const char* value(const char* str, int num = 0) {
        return !str || *str == 0 || *str == ',' ? nullptr : num != 0 ? value(next(str, num)) : *str == '=' ? trim(str + 1) : value(str + 1);
    }

    //template<class T>
    //constexpr T value(const char* str, int num = 0, T invalid_value = (T)~0) {
    //    return (!value_str(str, num) || (strtoll(value_str(str, num), 0, 0, 0, 3) == 3 && strtoll(value_str(str, num), 0, 0, 0, 4) == 4)) ? invalid_value : (T)strtoll(value_str(str, num), 0, 0, 0, invalid_value);
    //}

    constexpr int count(const char* str, int pos = 0) {
        return name(str, pos) ? count(str, pos + 1) : pos;
    }

    constexpr const char* name_or_value(const char* str, int num) {
        return num % 2 == 0 ? name(str, num / 2) : value(str, num / 2);
    }

    constexpr int names_char_at(const char* str, int index, int num = -1, int pos = 1, int nlen = 0) {
        return (pos > nlen) ?
            (name_or_value(str, num + 1) ? names_char_at(str, index, num + 1, 0, len(name_or_value(str, num + 1))) : 0) :
                (index == 0 ? (pos == nlen ? -1 : *(name_or_value(str, num) + pos)) : names_char_at(str, index - 1, num, pos + 1, nlen));
    }

    constexpr int names_len(const char* str, int pos = 0) {
        return names_char_at(str, pos) ? names_len(str, pos + 1) : pos;
    }

    constexpr char name_int_to_char(int ch) {
        return ch == -1 ? 0 : ch;
    }

    template <std::size_t len>
    class ArrayNameAccess {
    public:
        const char* _elements;

        constexpr ArrayNameAccess(const char* elements) : _elements(elements)
        {
        }

        constexpr char operator[](size_t pos)
        {
            return name_int_to_char(names_char_at(_elements, pos));
        }
    };

};
