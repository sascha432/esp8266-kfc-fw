/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// dynamic vector that stores 1-15x Storage::Value::* objects

#include <Arduino_compat.h>
#include <vector>
#include <PrintArgs.h>
#include "WebUI/Config.h"

#include "Utility/Debug.h"

#ifndef DEBUG_STORAGE_VECTOR
#define DEBUG_STORAGE_VECTOR                                1
#endif

#if DEBUG_STORAGE_VECTOR
#define IF_DEBUG_STORAGE_VECTOR(...)                        __VA_ARGS__
#define DEBUG_STORAGE_RANGE_CHECK(st, it, sz, ty, cn)       ::FormUI::Storage::Value::PrintHelper::verify_iterator(DebugContext_ctor(), st, it, sz, ty, cn)
#else
#define IF_DEBUG_STORAGE_VECTOR(...)
#define DEBUG_STORAGE_RANGE_CHECK(...)
#endif

namespace FormUI {

    namespace Storage {

        namespace Value {

//            class PrintHelper {
//            public:
//                static void print(IF_DEBUG_STORAGE_VECTOR(const Storage::Vector &vector,) ConstIterator &iterator, PrintInterface &output);
//
//#if DEBUG_STORAGE_VECTOR
//                static void verify_iterator(const DebugContext &ctx, const Vector &storage, ConstIterator iterator, size_t size, Type type = (Storage::Type)0xff, uint8_t count = 0xff);
//#endif
//            };

            template<Type _StorageType>
            class Base {
            public:
                static constexpr Type type = _StorageType;
            };

            template<Type _StorageType, typename _Tv = const char *>
            class Single : public Base<_StorageType> {
            public:
                Single(_Tv value) : _value(value) {}

                _Tv getValue() const {
                    return _value;
                }

                template <typename _Ta>
                void push_back(_Ta &&target) const {
                    std::copy_n(((VectorBase::const_pointer)&_value), sizeof(_value), target);
                }

                template <typename _Ta, typename _Tb>
                static _Ta pop_front(_Tb &iterator) {
                    _Tv value;
                    std::copy_n(iterator, sizeof(value), (VectorBase::pointer)&value);
                    iterator += sizeof(value);
                    return _Ta(value);
                }

            private:
                _Tv _value;
            };

            template<Type _StorageType, typename _Tk = const char *, typename _Tv = const char *>
            class KeyValue : public Single<_StorageType, _Tv> {
            public:
                using Single<_StorageType, _Tv>::getValue;

                KeyValue(_Tk key, _Tv value) : Single<_StorageType, _Tv>(value), _key(key) {}

                template <typename _Ta>
                void push_back(_Ta &&target) const {
                    Single<_StorageType, _Tv>::push_back(target);
                    std::copy_n(((VectorBase::const_pointer) & _key), sizeof(_key), target);
                }

                template <typename _Ta, typename _Tb>
                static _Ta pop_front(_Tb &iterator) {
                    _Tk key;
                    _Tv value;
                    std::copy_n(iterator, sizeof(value), (VectorBase::pointer)&value);
                    iterator += sizeof(value);
                    std::copy_n(iterator, sizeof(key), (VectorBase::pointer)&key);
                    iterator += sizeof(key);
                    return _Ta(key, value);
                }

                _Tk getKey() const {
                    return _key;
                }
            private:
                _Tk _key;
            };

            class Label : public Single<Type::LABEL> {
            public:
                using Single::Single;
                using Single::getValue;
                using Single::push_back;
                using Single::pop_front;
            };

            class LabelRaw : public Single<Type::LABEL_RAW> {
            public:
                using Single::Single;
                using Single::getValue;
                using Single::push_back;
                using Single::pop_front;
            };

            class SuffixText : public Single<Type::SUFFIX_TEXT> {
            public:
                using Single::Single;
                using Single::getValue;
                using Single::push_back;
                using Single::pop_front;
            };

            class SuffixHtml : public Single<Type::SUFFIX_HTML> {
            public:
                using Single::Single;
                using Single::getValue;
                using Single::push_back;
                using Single::pop_front;
            };

            class Attribute : public KeyValue<Type::ATTRIBUTE> {
            public:
                using KeyValue::KeyValue;
                using KeyValue::getKey;
                using KeyValue::getValue;
                using KeyValue::push_back;
                using KeyValue::pop_front;
            };

            class Option : public KeyValue<Type::OPTION> {
            public:
                using KeyValue::KeyValue;
                using KeyValue::getKey;
                using KeyValue::getValue;
                using KeyValue::push_back;
                using KeyValue::pop_front;
            };

            class OptionNumKey : public KeyValue<Type::OPTION_NUM_KEY, int32_t> {
            public:
                using KeyValue::getKey;
                using KeyValue::getValue;
                using KeyValue::push_back;
                using KeyValue::pop_front;

                OptionNumKey(int32_t key, const char *value) : KeyValue<Type::OPTION_NUM_KEY, int32_t>(key, value) {}
            };

            class AttributeMinMax : public KeyValue<Type::OPTION_NUM_KEY, int32_t, int32_t> {
            public:
                using KeyValue::push_back;
                using KeyValue::pop_front;

                AttributeMinMax(int32_t minValue, int32_t maxValue) : KeyValue<Type::OPTION_NUM_KEY, int32_t, int32_t>(minValue, maxValue) {}

                inline int32_t getMin() const {
                    return KeyValue::getKey();
                }

                inline int32_t getMax() const {
                    return KeyValue::getValue();
                }
            };

        }

        class TypeByte {
        public:
            TypeByte(Iterator iterator) : TypeByte(*iterator) {
            }

            TypeByte(ConstIterator iterator) : TypeByte(*iterator) {
            }

            TypeByte(uint8_t byte) : _byte(byte) {
#if DEBUG_KFC_FORMS_DISABLE_ASSERT == 0
                __assert();
#endif
            }

            TypeByte(Type type, size_t count) : _type(static_cast<uint8_t>(type)), _count((uint8_t)count - 1) {
#if DEBUG_KFC_FORMS_DISABLE_ASSERT == 0
                __assert();
#endif
            }

#if DEBUG_KFC_FORMS_DISABLE_ASSERT == 0
            void __assert() {
                __LDBG_assert_printf((count() > 0 && count() < kTypeMaxCount), "count=0 < %u(%u) < %u: out of range", count(), kTypeMaxCount);
                __LDBG_assert_printf(type() >= Storage::Type::MIN && type() < Storage::Type::MAX, "invalid type %u", type());
            }
#endif

            Iterator operator+(Iterator iterator) {
                return iterator + size();
            }
            ConstIterator operator+(ConstIterator iterator) {
                return iterator + size();
            }

            uint8_t operator--() {
                // handling 4 bit overflow of _count
                return (_count-- + 1) & 0xf;
            }

            inline Type type() const {
                return static_cast<Type>(_type);
            }

            inline size_t size() const {
                return (count() * item_size()) + sizeof(Type);
            }

            inline size_t item_size() const {
                return item_size(type());
            }

            inline uint8_t count() const {
                return _count + 1;
            }

            inline const char *name() const {
                return name(type());
            }

            inline uint8_t toByte() const {
                return _byte;
            }

            inline static size_t item_size(Type type) {
                switch (type) {
                case Type::ATTRIBUTE:
                    return sizeof(Value::Attribute);
                case Type::LABEL:
                    return sizeof(Value::Label);
                case Type::LABEL_RAW:
                    return sizeof(Value::LabelRaw);
                case Type::SUFFIX_TEXT:
                    return sizeof(Value::SuffixText);
                case Type::SUFFIX_HTML:
                    return sizeof(Value::SuffixHtml);
                case Type::OPTION:
                    return sizeof(Value::Option);
                case Type::OPTION_NUM_KEY:
                    return sizeof(Value::OptionNumKey);
                case Type::ATTRIBUTE_MIN_MAX:
                    return sizeof(Value::AttributeMinMax);
                default:
                    break;
                }
                return 0;
            }

            static const char *name(Type type) {
                switch (type) {
                case Type::ATTRIBUTE:
                    return PSTR("Attribute");
                case Type::LABEL:
                    return PSTR("Label");
                case Type::LABEL_RAW:
                    return PSTR("LabelRaw");
                case Type::SUFFIX_TEXT:
                    return PSTR("SuffixText");
                case Type::SUFFIX_HTML:
                    return PSTR("SuffixHtml");
                case Type::OPTION:
                    return PSTR("Option");
                case Type::OPTION_NUM_KEY:
                    return PSTR("OptionNumKey");
                case Type::ATTRIBUTE_MIN_MAX:
                    return PSTR("AttributeMinMax");
                default:
                    break;
                }
                return PSTR("NONE");
            }

        private:
            union {
                struct {
                    uint8_t _type: 4;
                    uint8_t _count: 4;
                };
                uint8_t _byte;
            };
        };

        template<typename _Ta, typename _Tb = Storage::ConstIterator>
        class SingleValueArgs {
        public:
            SingleValueArgs(_Tb &iterator, Storage::TypeByte tb) : _args{}, _ptr(_args) {
                while (--tb) {
                    auto value = _Ta::template pop_front<_Ta, _Tb>(iterator);
                    *_ptr++ = (uintptr_t *)value.getValue();
                }
            }

            operator uintptr_t **() {
                return &_args[0];
            }

            size_t size() const {
                return _ptr - _args;
            }

            void print(PrintInterface &output) {
                auto count = size();
                if (count == 1) {
                    output.printf_P(PrintArgs::FormatType::SINGLE_STRING, (const char *)_args[0]);
                }
                else {
                    output.vprintf_P(_args, size());
                }
            }

        private:
            uintptr_t *_args[Storage::kTypeMaxCount];
            uintptr_t **_ptr;
        };

        // converts FormUI::* to storage format
        class Vector : public VectorBase {
        public:
            using VectorBase::VectorBase;
            using VectorBase::begin;
            using VectorBase::end;

            //typedef bool(* Pred)(Type);

            inline static bool isAttribute(Type type) {
                return type == Type::ATTRIBUTE || type == Type::ATTRIBUTE_MIN_MAX;
            }

            inline static bool isLabel(Type type) {
                return type == Type::LABEL || type == Type::LABEL_RAW;
            }

            inline static bool isSuffix(Type type) {
                return type == Type::SUFFIX_TEXT || type == Type::SUFFIX_HTML;
            }

            inline static bool isOption(Type type) {
                return type == Type::OPTION || type == Type::OPTION_NUM_KEY;
            }

            inline static bool isMinMax(Type type) {
                return type == Type::ATTRIBUTE_MIN_MAX;
            }

            //template<typename _Ta, typename _Tb>
            //inline static _Ta pop_front(_Tb &iterator) {
            //    return _Ta::template pop_front<_Ta, _Tb>(++iterator);
            //}

            void dump(Print &output) const;

            void reserve_extend(size_t extra)
            {
                size_t newCapacity = size() + extra;
                size_t allocBlockSize = (newCapacity + 7) & ~7;     // use mallocs 8 byte block size
                reserve(allocBlockSize);
            }

            template<typename _Ta>
            void push_back(const _Ta &item)
            {
                reserve_extend(sizeof(item) + sizeof(_Ta::type));
#if KFC_FORMS_NO_DIRECT_COPY
                
                auto iterator = std::back_inserter<VectorBase>(*this);
                *iterator = static_cast<uint8_t>(_Ta::type);
                item.push_back(++iterator);
#else
                auto vSize = size();
                resize(vSize + sizeof(item) + sizeof(_Ta::type));
                auto ptr = (uint8_t *)data() + vSize;
                *ptr++ = static_cast<uint8_t>(_Ta::type);
                //std::copy_n((uint8_t *)&item, sizeof(item), ptr);
                memcpy(ptr, (uint8_t *)&item, sizeof(item));
#endif
            }

            void push_back(Type type, ValueStringVector::iterator begin, ValueStringVector::iterator end)
            {
                auto count = std::distance(begin, end);
                TypeByte tb(type, count);
                size_t vSize = size();
                reserve(vSize + tb.size());
#if KFC_FORMS_NO_DIRECT_COPY
                auto target = std::back_inserter<VectorBase>(*this);
                *target = tb.toByte();
                ++target;
                for (auto iterator = begin; iterator != end; ++iterator) {
                    Value::String(*iterator).push_back(target);
                }
#else
                static_assert(sizeof(Storage::Value::String) == sizeof(const char *), "size does not match");
                resize(vSize + tb.size());
                auto ptr = data() + vSize;
                *ptr++ = tb.toByte();
                auto beginPtr = (const char **)&(*begin);
                //std::copy(beginPtr, beginPtr + count, (const char **)ptr);
                memcpy(ptr, beginPtr, count);
#endif
            }

            template <class _Ta>
            class compare_type : public std::unary_function<_Ta, bool>
            {
            protected:
                _Ta _type;
            public:
                explicit compare_type(_Ta type) : _type(type) {}
                bool operator() (_Ta type) const {
                    return _type == type;
                }
            };

            template<typename _Ta>
            _Ta find(_Ta begin, _Ta end, Type type) const {
                return find_if<_Ta, compare_type<Type>>(begin, end, compare_type<Type>(type));
            }

            template<typename _Ta, typename _Pr>
            _Ta find_if(_Ta begin, _Ta end, _Pr pred) const {
                auto iterator = begin;
                while (iterator != end) {
                    TypeByte tb(iterator);
                    if (pred(tb.type())) {
                        return iterator;
                    }
                    iterator = tb + iterator;
                }
                return iterator;
            }

            template<typename _Ta, typename _Tb>
            void for_each(_Ta begin, Type type, _Tb func) const {
                auto iterator = find(begin, end(), type);
                while (iterator != end()) {
                    TypeByte tb(iterator);
                    if (tb.type() == type) {
                        func(++iterator, tb);
                    }
                    iterator = find(iterator, end(), type);
                }
            }

            template<typename _Ta, typename _Pr, typename _Tb>
            void for_each_if(_Ta begin, _Pr pred, _Tb func) const {
                auto iterator = find_if(begin, end(), pred);
                while (iterator != end()) {
                    TypeByte tb(iterator);
                    if (pred(tb.type())) {
                        func(++iterator, tb);
                    }
                    iterator = find_if(iterator, end(), pred);
                }
            }

            //ConstIterator find(ConstIterator iterator, ConstIterator end, Pred pred) const;
        };

    }

}

#include <debug_helper_disable.h>
