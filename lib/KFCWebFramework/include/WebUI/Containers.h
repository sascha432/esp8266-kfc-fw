/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <PrintString.h>
#include "Types.h"
#include "WebUI/Config.h"

#include "Utility/Debug.h"

namespace FormUI {

    inline namespace Container {

        class MixedString : public String
        {
        public:
            MixedString() :
                String(),
                _fpstr(nullptr)
            {
            }

            MixedString(const String &value) :
                String(value),
                _fpstr(nullptr)
            {
            }

            MixedString(String &&value) :
                String(std::move(value)),
                _fpstr(nullptr)
            {
            }

            MixedString(const __FlashStringHelper *value) :
                String(),
                _fpstr(value)
            {
            }

            const char *getValue() const {
                if (_fpstr) {
                    return (const char *)_fpstr;
                }
                else {
                    return c_str();
                }
            }

        private:
            const __FlashStringHelper *_fpstr;
        };

        class Mixed {
        public:
            enum class Type : uint8_t {
                NONE,
                NULLPTR = NONE,
                CSTR,
                FPSTR,
                STRING,
                CHAR,
                INT32,
                UINT32,
                FLOAT,
                MAX = 15,
                ENCODE_HTML_ATTRIBUTE		= 0x40,
                ENCODE_HTML_ENTITIES		= 0x80
            };

            Mixed() : _data(0), _type(Type::NULLPTR) {}

            Mixed(const Mixed &copy) : _data(copy._data), _type(copy._type) {
                _allocate(copy);
            }

            Mixed(Mixed &&move) noexcept : _data(std::exchange(move._data, 0)), _type(std::exchange(move._type, Type::NONE)) {}

            Mixed &operator=(const Mixed &copy) {
                _release();
                _type = copy._type;
                _data = copy._data;
                _allocate(copy);
                return *this;
            }

            Mixed &operator=(Mixed &&move) noexcept {
                _release();
                _data = std::exchange(move._data, 0);
                _type = std::exchange(move._type, Type::NONE);
                return *this;
            }

            Mixed(const __FlashStringHelper *value) : _fpStr(value), _type(Type::FPSTR) {}
            Mixed(const String &value) : _str(new String(value)), _type(Type::STRING) {}
            Mixed(String &&value) : _str(new String(std::move(value))), _type(Type::STRING) {}
            Mixed(double value) : _float((float)value), _type(Type::FLOAT) {}
            Mixed(float value) : _float(value), _type(Type::FLOAT) {}
            Mixed(char value) : _char(value), _type(Type::CHAR) {}
            Mixed(int32_t value) : _int(value), _type(Type::INT32) {}
            Mixed(uint32_t value) : _uint(value), _type(Type::UINT32) {}
            Mixed(const char *str) : _charPtr(strdup(str)), _type(Type::CSTR) {}

            ~Mixed() {
                _release();
            }

            inline void clear() {
                _release();
                _type = Type::NONE;
                _data = 0;
            }

            inline Type getType() const {
                return _mask(_type);
            }

            inline bool isSigned() const {
                return getType() == Type::INT32;
            }

            inline bool isUnsigned() const {
                return getType() == Type::UINT32;
            }

            // returns true for all integral types that can be retrieved with getInt()
            inline bool isInt() const {
                return isSigned() || isUnsigned() || isChar();
            }

            inline bool isFloat() const {
                return getType() == Type::FLOAT;
            }

            inline bool isChar() const {
                return getType() == Type::CHAR;
            }

            inline bool isFPStr() const {
                return getType() == Type::FPSTR;
            }

            inline bool isStringPtr() const {
                return getType() == Type::STRING;
            }

            inline bool isCStr() const {
                return getType() == Type::CSTR;
            }

            inline int32_t getInt() const {
                return _int;
            }

            inline int32_t getSigned() const {
                return _int;
            }

            inline uint32_t getUInt() const {
                return _uint;
            }

            inline uint32_t getUnsigned() const {
                return _int;
            }

            inline float getFloat() const {
                return _float;
            }

            inline char getChar() const {
                return _char;
            }

            inline const __FlashStringHelper *getFPString() const {
                return _fpStr;
            }

            inline String *getStringPtr() const {
                return _str;
            }

            inline const char *getCStr() const {
                return _cStr;
            }

            // precision is for convertig float to string
            inline String getString(int precision = 2) const {
                switch (getType()) {
                case Type::CSTR:
                    return String(_cStr);
                case Type::STRING:
                    return *_str;
                case Type::FLOAT:
                    return String(_float, precision);
                case Type::INT32:
                    return String(_int);
                case Type::UINT32:
                    return String(_uint);
                case Type::FPSTR:
                    return String(_fpStr);
                case Type::CHAR:
                    return String(_char);
                default:
                    break;
                }
                return String();
            }

    #if 1
            void dump(Print &output) const {
                if (_ptr == nullptr && getType() <= Type::STRING) {
                    output.print(F("<NULL>"));
                    return;
                }
                switch (getType()) {
                case Type::CSTR:
                    output.print(F("[strdup]"));
                    output.print(_cStr);
                    break;
                case Type::FPSTR:
                    output.print(F("[FPSTR]"));
                    output.print(_fpStr);
                    break;
                case Type::STRING:
                    output.print(F("[String *]"));
                    output.print(*_str);
                    break;
                case Type::INT32:
                    output.print(F("[int32_t]"));
                    output.print(_int);
                    break;
                case Type::UINT32:
                    output.print(F("[uint32_t]"));
                    output.print(_uint);
                    break;
                case Type::FLOAT:
                    output.print(F("[float]"));
                    output.print(_float, 6);
                    break;
                case Type::CHAR:
                    output.print(F("[char]"));
                    if (isprint(_char)) {
                        output.printf_P(PSTR("'%c' 0x%02x"), _char, (uint8_t)_char, (uint8_t)_char);
                    }
                    else {
                        output.printf_P(PSTR(" 0x%02x"), (uint8_t)_char, (uint8_t)_char);
                    }
                    break;
                default:
                    output.print(F("<NULL>"));
                    break;
                }
            }
    #endif

        private:
            inline void _allocate(const Mixed &copy) {
                if (copy._ptr) {
                    switch (getType()) {
                    case Type::CSTR:
                        _charPtr = copy._charPtr ? strdup(copy._charPtr) : strdup(emptyString.c_str());
                        break;
                    case Type::STRING:
                        _str = copy._str ? new String(*copy._str) : new String();
                        break;
                    default:
                        break;
                    }
                }
            }

            inline void _release() {
                if (_ptr) {
                    if (_isType(Type::CSTR)) {
                        free(_charPtr);
                    }
                    else if (_isType(Type::STRING)) {
                        delete _str;
                    }
                }
            }

            inline Type _mask(Type type) const {
                // not used
                //return static_cast<Type>(static_cast<uint8_t>(type) & 0x0f);
                return type;
            }

            inline bool _isType(Type type) const {
                return _mask(_type) == type;
            }

        public:
            // the typedef is just for validating the size and selecting the copy type
            typedef union {
                float _float;
                int32_t _int;
                uint32_t _uint;
                const char *_cStr;
                const __FlashStringHelper *_fpStr;
                String *_str;
                void *_ptr;
                char *_charPtr;
                char _char;
            } MixedUnion_t;

            using UnionCopyType = std::conditional<sizeof(MixedUnion_t) == sizeof(uint32_t), uint32_t, uint64_t>::type;

            // UnionCopyType is used to copy the entire data and needs to match the size
            static_assert(sizeof(MixedUnion_t) == sizeof(UnionCopyType), "union size invalid");

            union {
                float _float;
                int32_t _int;
                uint32_t _uint;
                const char *_cStr;
                const __FlashStringHelper *_fpStr;
                String *_str;
                void *_ptr;
                char *_charPtr;
                char _char;
                UnionCopyType _data{ 0 };
            };
            Type _type;
        };

        using MixedPair = std::pair<Mixed, Mixed>;

        using MixedVector = std::vector<Mixed>;

        using MixedStringVector = std::vector<MixedString>;

        using ListVector = std::vector<MixedPair>;

        static constexpr size_t MixedContainerUnionSize = sizeof(Mixed::MixedUnion_t);
        static constexpr size_t MixedContainerSize = sizeof(Mixed);
        static constexpr size_t MixedContainerPairSize = sizeof(MixedPair);

        class List : public ListVector {
        public:
            List() : ListVector() {}

            List(const List &list) : ListVector(list) {}

            List(List &&list) noexcept : ListVector(std::move(list)) {}

            List &operator=(const List &copy) {
                ListVector::operator=(copy);
                return *this;
            }

            List &operator=(List &&move) noexcept {
                ListVector::operator=(std::move(move));
                return *this;
            }

            // pass key value pairs as arguments
            // supports: String, const FlashStringHelper *, int, unsigned, float, enum class with underlying type int/unsigned, int16_t, int8_t etc..
            template <typename _Ta, typename... Args>
            List(_Ta t, Args &&... args) : ListVector() {
                static_assert((sizeof ...(args) + 1) % 2 == 0, "invalid number of pairs");
                reserve((sizeof ...(args) + 1) / 2);
                _addAll(t, std::forward<Args>(args)...);
            }

            template <typename _Ta, typename _Tb>
            void emplace_back(_Ta &&key, _Tb &&val) {
                reserve((size() + 2) & ~1); // allocate 8 byte blocks
                ListVector::emplace_back(std::move(_create(std::move(key))), std::move(_create(std::move(val))));
            }

            template <typename _Ta, typename _Tb>
            void push_back(const _Ta &key, const _Tb &val) {
                reserve((size() + 2) & ~1); // allocate 8 byte blocks
                ListVector::emplace_back(_passthrough(key), _passthrough(val));
            }

        private:

            template<typename _Ta>
            struct use_const_ref {
                static constexpr bool value = !std::is_enum<_Ta>::value && !std::is_integral<_Ta>::value && !std::is_floating_point<_Ta>::value && !std::is_same<_Ta, const __FlashStringHelper *>::value;
            };

            template <typename _Ta, typename _Tb = stdex::relaxed_underlying_type_t<_Ta>, typename std::enable_if<std::is_enum<_Ta>::value, int>::type = 0>
            inline _Tb _passthrough(_Ta t) {
                return static_cast<_Tb>(t);
            }

            template <typename _Ta, typename std::enable_if<use_const_ref<_Ta>::value, int>::type = 0>
            inline const _Ta &_passthrough(const _Ta &t) {
                return t;
            }


            inline const __FlashStringHelper *_passthrough(const __FlashStringHelper *fpstr) {
                return fpstr;
            }

            template <typename _Ta, typename std::enable_if<std::is_integral<_Ta>::value && std::is_signed<_Ta>::value, int>::type = 0>
            inline int32_t _passthrough(_Ta t) {
                return t;
            }

            template <typename _Ta, typename std::enable_if<std::is_integral<_Ta>::value && std::is_unsigned<_Ta>::value, int>::type = 0>
            inline uint32_t _passthrough(_Ta t) {
                return t;
            }

            inline int32_t _passthrough(char t) {
                return t;
            }

            inline float _passthrough(double t) {
                return (float)t;
            }

            inline float _passthrough(float t) {
                return t;
            }

            inline const char *_passthrough(const char t[]) {
                return t;
            }

            inline char *_passthrough(char t[]) {
                return t;
            }

            template <typename _Ta, typename _Tb = stdex::relaxed_underlying_type_t<_Ta>, typename std::enable_if<std::is_enum<_Ta>::value, int>::type = 0>
            inline _Tb _create(_Ta t) {
                return static_cast<_Tb>(t);
            }

            template <typename _Ta, typename std::enable_if<use_const_ref<_Ta>::value, int>::type = 0>
            inline const _Ta &_create(const _Ta &t) {
                return t;
            }

            inline const __FlashStringHelper *_create(const __FlashStringHelper *fpstr) {
                return fpstr;
            }

            template <typename _Ta, typename std::enable_if<std::is_integral<_Ta>::value && std::is_signed<_Ta>::value, int>::type = 0>
            inline int32_t _create(_Ta t) {
                return t;
            }

            template <typename _Ta, typename std::enable_if<std::is_integral<_Ta>::value && std::is_unsigned<_Ta>::value, int>::type = 0>
            inline uint32_t _create(_Ta t) {
                return t;
            }

            inline int32_t _create(char t) {
                return t;
            }

            inline float _create(double t) {
                return (float)t;
            }

            inline float _create(float t) {
                return t;
            }

            inline const char *_create(const char t[]) {
                return t;
            }

            inline char *_create(char t[]) {
                return t;
            }

            template <typename _Ta, typename std::enable_if<use_const_ref<_Ta>::value, int>::type = 0>
            inline Mixed _create(_Ta &&t) {
                return Mixed(std::move(t));
            }

    #if 1
        public:
            void dump(Print &output, const char *str = nullptr) const {
                if (str) {
                    output.printf_P(PSTR("list=%s size=%u capacity=%u\n"), str, size(), capacity());
                }
                else {
                    output.printf_P(PSTR("list=<unnamed> size=%u capacity=%u\n"), size(), capacity());
                }
                for (const auto &item : *this) {
                    item.first.dump(output);
                    output.print('=');
                    item.second.dump(output);
                    output.println();
                }
            }

        private:
    #endif

            void _addAll() {
            }

            template <typename _Ta, typename _Tb, typename... Args>
            void _push(_Ta &&key, _Tb &&val, Args &&... args) {
                ListVector::emplace_back(std::move(_create(std::move(key))), std::move(_create(std::move(val))));
                _addAll(args...);
            }

            template <typename _Ta, typename... Args>
            void _addAll(_Ta &&t, Args &&... args) {
                _push(std::move(t), args...);
            }

        };

        class Label : public MixedString
        {
        public:
            using MixedString::MixedString;
        };

        class RawLabel : public Label
        {
        public:
            using Label::Label;
        };

        class Suffix : public MixedString {
        public:
            using MixedString::MixedString;
        };

        class InlineSuffix : public MixedString {
        public:
            InlineSuffix(const __FlashStringHelper *value) : MixedString(PrintString(F("<span class=\"input-group-text inline hidden\">%s</span>"), value)) {}
            InlineSuffix(const String &value) : InlineSuffix(reinterpret_cast<const __FlashStringHelper *>(value.c_str())) {}
            InlineSuffix(const char *value) : InlineSuffix(reinterpret_cast<const __FlashStringHelper *>(value)) {}
        };

        class SuffixHtml : public Suffix {
        public:
            using Suffix::Suffix;
        };

        class ZeroconfSuffix : public SuffixHtml {
        public:
            ZeroconfSuffix() : SuffixHtml(F("<button type=\"button\" class=\"resolve-zerconf-button btn btn-default\" data-color=\"primary\">Resolve Zeroconf</button>")) {}
        };

        class CheckboxButtonSuffix {
        public:
            using FormField = ::FormUI::Field::BaseField;

        public:
            CheckboxButtonSuffix(const FormField &hiddenField, const String &label, const __FlashStringHelper *onIcons = nullptr, const __FlashStringHelper *offIcons = nullptr)
            {
                initButton(hiddenField, onIcons, offIcons);
                _items.emplace_back(label);
            }
            CheckboxButtonSuffix(const FormField &hiddenField, const __FlashStringHelper *label, const __FlashStringHelper *onIcons = nullptr, const __FlashStringHelper *offIcons = nullptr)
            {
                initButton(hiddenField, onIcons, offIcons);
                _items.emplace_back(label);
            }

        protected:
            CheckboxButtonSuffix() {}

            void initButton(const FormField &hiddenField, const __FlashStringHelper *onIcons, const __FlashStringHelper *offIcons);

        protected:
            friend WebUI::BaseUI;

            MixedStringVector _items;
        };

        class SelectSuffix : public CheckboxButtonSuffix {
        public:
            SelectSuffix(const FormField &hiddenField);

        // // options will be copied from the hidden select field
        // protected:
        //     friend WebUI::BaseUI;
        //     List _options;
        };

        class IntMinMax
        {
        public:
            IntMinMax(int32_t aMin, int32_t aMax) : _minValue(aMin), _maxValue(aMax) {}

            inline FormUI::Storage::Value::AttributeMinMax create() const {
                return FormUI::Storage::Value::AttributeMinMax(_minValue, _maxValue);
            }

        private:
            int32_t _minValue;
            int32_t _maxValue;
            // int32_t _step: 31;
            // int32_t _hasStep: 1;
        };

        inline IntMinMax MinMax(int32_t aMin, int32_t aMax) {
            return IntMinMax(aMin, aMax);
        }

        template<typename _Ta>
        class AttributeTmpl {
        public:
        };

        template<>
        class AttributeTmpl<int32_t> {
        public:
            AttributeTmpl(const __FlashStringHelper *key, int32_t value) : _key(key), _value(value) {}

            void push_back(FormUI::Storage::Vector &storage, FormUI::WebUI::BaseUI &ui) const;

        private:
            friend WebUI::BaseUI;

            const __FlashStringHelper *_key;
            int32_t _value;
        };

        template<>
        class AttributeTmpl<const __FlashStringHelper *> {
        public:
            AttributeTmpl(const __FlashStringHelper *key, const __FlashStringHelper *value) : _key(key), _value(value) {}

            void push_back(FormUI::Storage::Vector &storage, FormUI::WebUI::BaseUI &ui) const;

        private:
            const __FlashStringHelper *_key;
            const __FlashStringHelper *_value;
        };

        template<>
        class AttributeTmpl<String> {
        public:
            AttributeTmpl(const __FlashStringHelper *key, const String &value) : _key(key), _value(value) {}
            AttributeTmpl(const __FlashStringHelper *key, String &&value) : _key(key), _value(std::move(value)) {}

            void push_back(FormUI::Storage::Vector &storage, FormUI::WebUI::BaseUI &ui) const;

        private:
            const __FlashStringHelper *_key;
            String _value;
        };

        using IntAttribute = AttributeTmpl<int32_t>;
        using StringAttribute = AttributeTmpl<String>;
        using FPStringAttribute = AttributeTmpl<const __FlashStringHelper *>;


        inline FPStringAttribute Attribute(const __FlashStringHelper *key, const __FlashStringHelper *value) {
            return FPStringAttribute(key, value);
        }

        inline StringAttribute Attribute(const __FlashStringHelper *key, const String &value) {
            return StringAttribute(key, value);
        }

        inline StringAttribute Attribute(const __FlashStringHelper *key, String &&value) {
            return StringAttribute(key, std::move(value));
        }

        inline IntAttribute Attribute(const __FlashStringHelper *key, int value) {
            return IntAttribute(key, value);
        }

        inline IntAttribute Attribute(const __FlashStringHelper *key, long value) {
            return IntAttribute(key, (int)value);
        }

        inline IntAttribute PlaceHolder(int32_t placeholder) {
            return IntAttribute(F("placeholder"), placeholder);
        }

        inline StringAttribute PlaceHolder(double placeholder, uint8_t digits) {
            return StringAttribute(F("placeholder"), String(placeholder, digits));
        }

        inline FPStringAttribute PlaceHolder(const __FlashStringHelper *placeholder) {
            return FPStringAttribute(F("placeholder"), placeholder);
        }

        inline StringAttribute PlaceHolder(const String &placeholder) {
            return StringAttribute(F("placeholder"), placeholder);
        }

        inline StringAttribute PlaceHolder(String &&placeholder) {
            return StringAttribute(F("placeholder"), std::move(placeholder));
        }

        class ReadOnlyAttribute : public FPStringAttribute
        {
        public:
            ReadOnlyAttribute() : FPStringAttribute(FSPGM(readonly), FSPGM(readonly)) {}
        };

        class DisabledAttribute : public FPStringAttribute
        {
        public:
            DisabledAttribute() : FPStringAttribute(FSPGM(disabled), FSPGM(disabled)) {}
        };

        class BoolItems : public List
        {
        public:
            BoolItems() : List(0, FSPGM(Disabled), 1, FSPGM(Enabled)) {}
            BoolItems(const __FlashStringHelper *pTrue, const __FlashStringHelper *pFalse) : List(0, pFalse, 1, pTrue) {}
            BoolItems(const String &pTrue, const String &pFalse) : List(0, pFalse, 1, pTrue) {}
            BoolItems(String &&pTrue, String &&pFalse)  : List(0, std::move(pFalse), 1, std::move(pTrue)) {}
        };

        // _Ta can be any container
        // it gets added when condition _Ta evaluates to true
        template<typename _Tb, typename _Ta = bool>
        class Conditional {
        public:
            Conditional(_Ta condition, const _Tb &value) : _value(value), _condition(condition) {}
            Conditional(_Ta condition, _Tb &&value) : _value(std::move(value)), _condition(condition) {}

            template <typename... Args>
            Conditional(_Ta condition, Args &&... args) : _value(args...), _condition(condition) {}

            bool isConditionTrue() const {
                return (_condition);
            }

        private:
            friend WebUI::BaseUI;

            _Tb _value;
            _Ta _condition;
        };

    }

}

#include <debug_helper_disable.h>
