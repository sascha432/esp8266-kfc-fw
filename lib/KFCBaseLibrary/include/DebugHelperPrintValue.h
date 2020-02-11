/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "TypeName.h"

class DebugHelperPrintValue : public Print
{
public:
    operator bool() const {
        return DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE;
    }

    virtual void printPrefix() = 0;

    virtual size_t write(uint8_t ch) {
        if (*this) {
            return DEBUG_OUTPUT.write(ch);
        }
        return 0;
    }
    virtual size_t write(const uint8_t* buffer, size_t size) override {
        if (*this) {
            return DEBUG_OUTPUT.write(buffer, size);
        }
        return 0;
    }

    template <class T, class UT>
    void printType() {
        bool ptr = true;
        print('(');
        if (TypeName<UT>::sname()) {
            print(TypeName<UT>::sname());
        }
        else if (TypeName<T>::sname()) {
            print(TypeName<T>::sname());
            ptr = false;
        }
        else {
            print(F("object"));
        }
        if (std::is_reference<T>::value) {
            print(F("&"));
        }
        if (ptr && std::is_pointer<T>::value) {
            print(F("*"));
        }
        print(')');
    }

    void printValue(const char* value) {
        if (value) {
            printf_P(PSTR("'%s'"), value);
        }
        else {
            print(F("nullptr"));
        }
    }

    void printValue(char* value) {
        printValue((const char*)value);
    }

    template <class T>
    void printValue(T* value) {
        if (TypeName<T*>::format()) {
            printf_P(reinterpret_cast<PGM_P>(TypeName<T*>::format()), value);
        }
        else {
            printf_P(PSTR("%p"), value);
        }
    }

    template <class T>
    void printValue(T& value) {
        if (TypeName<T&>::format()) {
            printf_P(reinterpret_cast<PGM_P>(TypeName<T&>::format()), value);
        }
        else {
            printf_P(PSTR("%p"), value);
        }
    }

    //template <class T, typename std::enable_if<std::is_enum<T>::value>::type>
    //void printValue(T value) {
    //    printf_P(PSTR("%d"), static_cast<int>(value));
    //}

    //template <class T, typename std::enable_if<std::is_enum<T>::value>::type>
    //void printValue(T &value) {
    //    printf_P(PSTR("%d"), static_cast<int>(value));
    //}

    template <class T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
    void printValue(T value) {
        if (TypeName<T>::format()) {
            printf_P(reinterpret_cast<PGM_P>(TypeName<T>::format()), value);
        }
        else {
            printf_P(PSTR("%p"), value);
        }
    }

    nullptr_t printResult(nullptr_t value) {
        printPrefix();
        printf_P(PSTR("result nullptr\n"));
        return value;
    }

    template<class T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
    T printResult(T value) {
        printPrefix();
        print(F("result "));
        printType<T, T>();
        printValue(value);
        println();
        return value;
    }

    template<class T>
    T* printResult(T* value) {
        printPrefix();
        print(F("result "));
        printType<T*, T>();
        printValue(value);
        println();
        return value;
    }

    template<class T>
    T& printResult(T& value) {
        printPrefix();
        print(F("result "));
        printType<T&, T>();
        printValue(value);
        println();
        return value;
    }

    template<class T>
    const T* printResult(const T* value) {
        printPrefix();
        print(F("result "));
        printType<const T*, T>();
        printValue(value);
        println();
        return value;
    }

    template<class T>
    const T& printResult(const T& value) {
        printPrefix();
        print(F("result "));
        printType<const T&, T>();
        printValue(value);
        println();
        return value;
    }
};
