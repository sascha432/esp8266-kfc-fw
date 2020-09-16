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
                INT32,
                UINT32,
                DOUBLE,
                MAX = 15,
                ENCODE_HTML_ATTRIBUTE		= 0x40,
                ENCODE_HTML_ENTITIES		= 0x80
            };

            Mixed() :
                _ptr(nullptr),
                _type(Type::NULLPTR)
            {
            }

            Mixed(const Mixed &copy) :
                _ptr(copy._ptr),
                _type(copy._type)
            {
                _copy(copy);
            }
            Mixed &operator=(const Mixed &copy) {
                _release();
                _type = copy._type;
                _ptr = copy._ptr;
                _copy(copy);
                return *this;
            }

            Mixed(Mixed &&move) noexcept :
                _ptr(std::exchange(move._ptr, nullptr)),
                _type(std::exchange(move._type, Type::NONE))
            {}
            Mixed &operator=(Mixed &&move) noexcept {
                _release();
                _type = std::exchange(move._type, Type::NONE);
                _ptr = std::exchange(move._ptr, nullptr);
                return *this;
            }

            Mixed(const String &value) :
                _str(new String(value)),
                _type(Type::STRING)
            {}
            Mixed(String &&value) :
                _str(new String(std::move(value))),
                _type(Type::STRING)
            {}
            Mixed(double value) :
                _double((float)value),
                _type(Type::DOUBLE)
            {}
            Mixed(int32_t value) :
                _int(value),
                _type(Type::INT32)
            {}
            Mixed(uint32_t value) :
                _uint(value),
                _type(Type::UINT32)
            {}
            ~Mixed() {
                _release();
            }

            inline void clear() {
                _release();
                _type = Type::NONE;
            }

            Type getType() const {
                return _mask(_type);
            }

            bool isInt() const {
                return getType() == Type::INT32 || getType() == Type::UINT32;
            }

            bool isFPStr() const {
                return getType() == Type::FPSTR;
            }

            bool isStringPtr() const {
                return getType() == Type::STRING;
            }

            bool isCStr() const {
                return getType() == Type::CSTR;
            }

            int32_t getInt() const {
                return _int;
            }

            uint32_t getUInt() const {
                return _uint;
            }

            const __FlashStringHelper *getFPString() const {
                return _fpStr;
            }

            String *getStringPtr() const {
                return _str;
            }

            const char *getCStr() const {
                return _cStr;
            }

            String getString() const {
                switch (getType()) {
                case Type::CSTR:
                    return String(_cStr);
                case Type::STRING:
                    return *_str;
                case Type::DOUBLE:
                    return String(_double);
                case Type::INT32:
                    return String(_int);
                case Type::UINT32:
                    return String(_uint);
                case Type::FPSTR:
                    return String(_fpStr);
                default:
                    break;
                }
                return String();
            }

    #if 0
            void dump(Print &output) const {
                if (_ptr == nullptr && getType() <= Type::STRING) {
                    output.print(F("<NULL>"));
                    return;
                }
                switch (getType()) {
                case Type::CSTR:
                    output.print(_cStr);
                    break;
                case Type::FPSTR:
                    output.print(_fpStr);
                    break;
                case Type::STRING:
                    output.print(*_str);
                    break;
                case Type::INT32:
                    output.print(_int);
                    break;
                case Type::UINT32:
                    output.print(_uint);
                    break;
                case Type::DOUBLE:
                    output.print(_double);
                    break;
                }
            }
    #endif

        private:
            inline void _copy(const Mixed &copy) {
                if (copy._ptr) {
                    switch (getType()) {
                    case Type::CSTR:
                        _cStr = copy._cStr ? strdup(copy._cStr) : strdup(emptyString.c_str());
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
                        free(_ptr);
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

            union {
                float _double;
                int32_t _int;
                uint32_t _uint;
                const char *_cStr;
                const __FlashStringHelper *_fpStr;
                String *_str;
                void *_ptr;
            };
            Type _type;
        };

        using MixedPair = std::pair<Mixed, Mixed>;

        using MixedVector = std::vector<Mixed>;

        using MixedStringVector = std::vector<MixedString>;

        using ListVector = std::vector<MixedPair>;

        static constexpr size_t MixedContainerSize = sizeof(Mixed);
        static constexpr size_t MixedContainerPairSize = sizeof(MixedPair);

        class List : public ListVector {
        public:
            List() : ListVector() {}

            List &operator=(const List &items) {
                clear();
                reserve(items.size());
                for (const auto &item : items) {
                    ListVector::push_back(item);
                }
                return *this;
            }

            List &operator=(List &&items) noexcept {
                ListVector::swap(items);
                return *this;
            }

            // pass key value pairs as arguments
            // supports: String, const FlashStringHelper *, int, unsigned, float, enum class with underlying type int/unsigned, int16_t, int8_t etc..
            template <typename... Args>
            List(Args &&... args) : ListVector() {
                static_assert(sizeof ...(args) % 2 == 0, "invalid number of pairs");
                reserve((sizeof ...(args)) / 2);
                _addAll(args...);
            }

            template <typename _Ta, typename _Tb>
            void emplace_back(_Ta &&key, _Tb &&val) {
                ListVector::emplace_back(std::move(_create(std::move(key))), std::move(_create(std::move(val))));
            }

            template <typename _Ta, typename _Tb>
            void push_back(const _Ta &key, const _Tb &val) {
                ListVector::emplace_back(_passthrough(key), _passthrough(val));
            }

        private:
            template <typename _Ta, typename _Tb = std::relaxed_underlying_type_t<_Ta>, typename std::enable_if<std::is_enum<_Ta>::value, int>::type = 0>
            inline _Tb _passthrough(_Ta t) {
                return static_cast<_Tb>(t);
            }

            template <typename _Ta, typename std::enable_if<!std::is_enum<_Ta>::value, int>::type = 0>
            inline const _Ta &_passthrough(const _Ta &t) {
                return t;
            }

            template <typename _Ta, typename _Tb = std::relaxed_underlying_type_t<_Ta>, typename std::enable_if<std::is_enum<_Ta>::value, int>::type = 0>
            inline Mixed _create(_Ta t) {
                return Mixed(static_cast<_Tb>(t));
            }

            template <typename _Ta, typename std::enable_if<!std::is_enum<_Ta>::value, int>::type = 0>
            inline Mixed _create(const _Ta &t) {
                return Mixed(t);
            }

            template <typename _Ta, typename std::enable_if<!std::is_enum<_Ta>::value, int>::type = 0>
            inline Mixed _create(_Ta &&t) {
                return Mixed(std::move(t));
            }

    #if 0
            void dump(Print &output) const {
                for (const auto &item : *this) {
                    item.first.dump(output);
                    output.print('=');
                    item.second.dump(output);
                    output.println();
                }
            }
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
            CheckboxButtonSuffix(FormField &hiddenField, const String &label, const __FlashStringHelper *onIcons = nullptr, const __FlashStringHelper *offIcons = nullptr)
            {
                initButton(hiddenField, onIcons, offIcons);
                _items.emplace_back(label);
            }
            CheckboxButtonSuffix(FormField &hiddenField, const __FlashStringHelper *label, const __FlashStringHelper *onIcons = nullptr, const __FlashStringHelper *offIcons = nullptr)
            {
                initButton(hiddenField, onIcons, offIcons);
                _items.emplace_back(label);
            }

        protected:
            void initButton(FormField &hiddenField, const __FlashStringHelper *onIcons, const __FlashStringHelper *offIcons);

        private:
            friend WebUI::BaseUI;

            MixedStringVector _items;
        };

        class IntMinMax
        {
        public:
            IntMinMax(int32_t aMin, int32_t aMax) : _minValue(aMin), _maxValue(aMax) {}

        private:
            friend WebUI::BaseUI;

            int32_t _minValue;
            int32_t _maxValue;
        };

        inline IntMinMax MinMax(int32_t aMin, int32_t aMax) {
            return IntMinMax(aMin, aMax);
        }

        class StringAttribute
        {
        public:
            StringAttribute(const __FlashStringHelper *key, const String &value) : _key(key), _value(value) {}
            StringAttribute(const __FlashStringHelper *key, String &&value) : _key(key), _value(std::move(value)) {}

        private:
            friend WebUI::BaseUI;

            const __FlashStringHelper *_key;
            String _value;
        };

        class FPStringAttribute
        {
        public:
            FPStringAttribute(const __FlashStringHelper *key, const __FlashStringHelper *value) : _key(key), _value(value) {}

        private:
            friend WebUI::BaseUI;

            const __FlashStringHelper *_key;
            const __FlashStringHelper *_value;
        };

        inline FPStringAttribute Attribute(const __FlashStringHelper *key, const __FlashStringHelper *value) {
            return FPStringAttribute(key, value);
        }

        inline StringAttribute Attribute(const __FlashStringHelper *key, const String &value) {
            return StringAttribute(key, value);
        }

        inline StringAttribute Attribute(const __FlashStringHelper *key, String &&value) {
            return StringAttribute(key, std::move(value));
        }


        inline StringAttribute PlaceHolder(int32_t placeholder) {
            return StringAttribute(F("placeholder"), String(placeholder));
        }

        inline StringAttribute PlaceHolder(uint32_t placeholder) {
            return StringAttribute(F("placeholder"), String(placeholder));
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

        class ReadOnly : public StringAttribute
        {
        public:
            ReadOnly() : StringAttribute(FSPGM(readonly), std::move(String())) {}
        };

        class BoolItems
        {
        public:
            BoolItems() : _false(FSPGM(Disabled)), _true(FSPGM(Enabled)) {}
            BoolItems(const String &pTrue, const String &pFalse) : _false(pFalse), _true(pTrue) {}
            BoolItems(String &&pTrue, String &&pFalse) : _false(std::move(pFalse)), _true(std::move(pTrue)) {}

        private:
            friend WebUI::BaseUI;

            String _false;
            String _true;
        };

        template<class _Ta>
        class Conditional {
        public:
            Conditional(bool condition, const _Ta &value) : _value(value), _condition(condition) {}
            Conditional(bool condition, _Ta &&value) : _value(std::move(value)), _condition(condition) {}

        private:
            friend WebUI::BaseUI;

            _Ta _value;
            bool _condition: 1;
        };

        class ConditionalAttribute : public Conditional<StringAttribute> {
        public:
            ConditionalAttribute(bool condition, const __FlashStringHelper *key, const String &value) : Conditional(condition, Attribute(key, value)) {}
            ConditionalAttribute(bool condition, const __FlashStringHelper *key, String &&value) : Conditional(condition, Attribute(key, std::move(value))) {}
        };

    }

}

#include <debug_helper_disable.h>
