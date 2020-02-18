/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "TypeName.h"
#include "misc.h"

#define DEBUG_DUMPER_ARGS(...)              F(_STRINGIFY(__VA_ARGS__)), __VA_ARGS__

template <typename T>
class HasToString
{
private:
    typedef char YesType[1];
    typedef char NoType[2];

    template <typename C> static YesType &test(decltype(&C::toString));
    template <typename C> static NoType &test(...);

public:
    enum { value = sizeof(test<T>(0)) == sizeof(YesType) };
};

class DebugDumper {
public:
    typedef enum {
        NONE = 0,
        ADD_TYPE = 0x01,            // display type
        NEWLINE = 0x02,             // add new line after output
        NULLPTR = 0x04,             // display null pointers
    } OptionsType;

    typedef std::vector<String> StringVector;

    DebugDumper(Print &output, int options = NEWLINE|NULLPTR) : _output(output), _options(static_cast<OptionsType>(options)) {
    }

    void setOptions(int options) {
        _options = static_cast<OptionsType>(options);
    }

    template<typename... Args>
    void dumpArgs(const __FlashStringHelper *argStr, Args... args) {
        auto size = sizeof... (Args);
        StringVector names;
        names.reserve(size);

        using SplitType = split::SplitFlagsType;
        split::split_P((PGM_P)argStr, ',', split::vector_callback, &names, SplitType::_PROGMEM|SplitType::TRIM);
        for (auto &name : names) {
            _stripName(name);
        }

        _args.reserve(size);
        _name = names.begin();
        dump_arg(args...);

        auto iterator = _args.begin();
        if (iterator != _args.end()) {
            _output.print(*iterator);
            while (++iterator != _args.end()) {
                _output.print(F(", "));
                _output.print(*iterator);
            }
        }
        if (_options & OptionsType::NEWLINE) {
            _output.println();
        }
        _args.clear();
    }

private:
    void _stripName(String &name) {
        int pos;
        if (name.indexOf(F("_cast")) != -1) {
            if (-1 != (pos = name.indexOf('<'))) {
                if (-1 != (pos = name.indexOf('>', pos))) {
                    name.remove(0, pos + 1);
                }
            }
        }
        else if (-1 != (pos = name.indexOf(')'))) {
            name.remove(0, pos + 1);
        }
        String_trim(name, "() \t");
    }

    void add(std::nullptr_t) {
        if (_options & OptionsType::NULLPTR) {
            String str;
            if (String_equals(*_name, TypeName<std::nullptr_t>::sname())) {
                str = F("unnamed");
            }
            else {
                str = *_name;
            }
            str += F("=nullptr");
            _args.emplace_back(str);
        }
        ++_name;
    }

    template<typename T>
    constexpr const __FlashStringHelper *_getTypeName() {
        return TypeName<T>::sname() ? TypeName<T>::sname() :
            TypeName<typename std::remove_cv<T>::type>::sname() ? TypeName<typename std::remove_cv<T>::type>::sname() :
            nullptr;
    }

    bool _invalidVarName(const char *ptr) const {
        static const char *extra = "_->.*()";
        if (!isalpha(*ptr) && !strchr(extra, *ptr)) {
            return false;
        }
        ptr++;
        while (*ptr) {
            if (!isalnum(*ptr) && !strchr(extra, *ptr)) {
                return false;
            }
            ptr++;
        }
        return true;
    }

    String _toString(const String &value) {
        return value;
    }

    String _toString(const void *value) {
        char buf[16];
        snprintf_P(buf, sizeof(buf), PSTR("%p"), value);
        return buf;
    }

    String _toString(bool value) {
        return value ? F("true") : F("false");
    }

    template<typename T, typename std::enable_if<std::is_class<T>::value && HasToString<T>::value, int>::type = 0>
    String _toString(const T &value) {
        return value.toString();
    }

    template<typename T, typename std::enable_if<std::is_class<T>::value && !HasToString<T>::value, int>::type = 0>
    String _toString(const T &value) {
        return F("<>");
    }

    template<typename T, typename std::enable_if<std::is_scalar<T>::value && !std::is_same<T, void *>::value && !std::is_same<T, bool>::value, int>::type = 0>
    String _toString(const T &value) {
        return String(value);
    }

    template<typename T>
    void add(T &value) {
        auto name = _getTypeName<T>();
        String str;
        String valueStr;
        valueStr = _toString(value);
        if (std::is_same<T, const char *>::value || std::is_same<T, char *>::value) {
            valueStr = String('"') + valueStr + String('"');
        }
        if (!_invalidVarName(_name->c_str())) {
            str = F("unnamed");
        }
        else {
            str += *_name;
        }
        if (_options & OptionsType::ADD_TYPE) {
            str += '<';
            if (!std::is_scalar<T>::value) {
                str += F("object");
            }
            else if (name) {
                str += name;
            }
            else {
                str += F("unknown");
            }
            str += '>';
            str += '=';
            str += valueStr;
        }
        else {
            str += '=';
            str += valueStr;
        }
        _args.push_back(str);
        ++_name;
    }

    void dump_arg() {
    }

    template<typename T>
    void dump_arg(T &value) {
        add(value);
    }

    template<typename T>
    void dump_arg(const T &value) {
        add(value);
    }

    template<typename T, typename... Args>
    void dump_arg(const T &value, Args... args) {
        add(value);
        dump_arg(args...);
    }

private:
    StringVector _args;
    StringVector::iterator _name;
    Print &_output;
    OptionsType _options;
};
