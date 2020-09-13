/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// dynamic vector that stores 1-15x int, const char *, PGM_P and const char * pairs per entry

#include <Arduino_compat.h>
#include <vector>
#include "FormUIConfig.h"
#include "FormUIItems.h"

namespace FormUI {

    class UI;
    using PrintInterface = PrintArgs;

    using ItemsStorageVector = std::vector<uint8_t>;

    // converts FormUI::* to storage format
    class ItemsStorage : public ItemsStorageVector {
    public:
        using ItemsStorageVector::ItemsStorageVector;
        using ItemsStorageVector::begin;
        using ItemsStorageVector::end;
        using Vector = ItemsStorageVector;
        using Iterator = Vector::iterator;
        using ConstIterator = Vector::const_iterator;

        enum class StorageType : uint8_t {
            LABEL,
            LABEL_RAW,
            ATTRIBUTE,
            ATTRIBUTE_MIN_MAX,
            SUFFIX_TEXT,
            SUFFIX_HTML,
            OPTION,
            OPTION_NUM_KEY,
            MAX = 15
        };

        typedef bool(* Pred)(StorageType);

        inline static bool isAttribute(ItemsStorage::StorageType type) {
            return type == StorageType::ATTRIBUTE || type == StorageType::ATTRIBUTE_MIN_MAX;
        }

        inline static bool isLabel(ItemsStorage::StorageType type) {
            return type == StorageType::LABEL || type == StorageType::LABEL_RAW;
        }

        inline static bool isSuffix(ItemsStorage::StorageType type) {
            return type == StorageType::SUFFIX_TEXT || type == StorageType::SUFFIX_HTML;
        }

        inline static bool isOption(ItemsStorage::StorageType type) {
            return type == StorageType::OPTION || type == StorageType::OPTION_NUM_KEY;
        }

        inline static bool isMinMax(ItemsStorage::StorageType type) {
            return type == StorageType::ATTRIBUTE_MIN_MAX;
        }

        inline static StorageType getType(uint8_t byte) {
            return static_cast<StorageType>(byte & 0x0f);
        }

        inline static uint8_t getCount(uint8_t byte) {
            return (byte >> 4) + 1;
        }

        inline static StorageType getType(ConstIterator iterator) {
            return static_cast<StorageType>(*iterator & 0x0f);
        }

        inline static uint8_t getCount(ConstIterator iterator) {
            return (*iterator >> 4) + 1; 
        }

        template<typename _Ta>
        inline static const _Ta &getData(ConstIterator iterator) {
            return *reinterpret_cast<const _Ta *>(&(*(iterator + 1)));
        }

        static inline ConstIterator advanceIterator(ConstIterator iterator) {
            auto byte = *iterator;
            return iterator + (getSizeOf(getType(byte)) * getCount(byte)) + sizeof(byte);
        }

        static inline size_t getSizeOf(StorageType type) {
            switch (type) {
            case StorageType::ATTRIBUTE:
                return sizeof(Attribute);
            case StorageType::LABEL:
                return sizeof(Label);
            case StorageType::LABEL_RAW:
                return sizeof(LabelRaw);
            case StorageType::SUFFIX_TEXT:
                return sizeof(SuffixText);
            case StorageType::SUFFIX_HTML:
                return sizeof(SuffixHtml);
            case StorageType::OPTION:
                return sizeof(Option);
            case StorageType::OPTION_NUM_KEY:
                return sizeof(OptionNumKey);
            case StorageType::ATTRIBUTE_MIN_MAX:
                return sizeof(AttributeMinMax);
            }
            __LDBG_assert_printf(false, "type=%u", type);
            return 1;
        }

        class PrintValue {
        public:
            static void print(ConstIterator &iterator, PrintInterface &output);
        };

        template<StorageType _StorageType>
        class NoValue {
        public:
            static constexpr StorageType type = _StorageType;
        };

        template<StorageType _StorageType>
        class SingleValue : public NoValue<_StorageType> {
        public:
            SingleValue(const char *value) : _value(value) {}

            const char *getValue() const {
                return _value;
            }

        private:
            const char *_value;
        };

        template<StorageType _StorageType>
        class KeyValuePair : public SingleValue<_StorageType> {
        public:
            using SingleValue<_StorageType>::getValue;

            KeyValuePair(const char *key, const char *value) : SingleValue<_StorageType>(value), _key(key) {}

            const char *getKey() const {
                return _key;
            }
        private:
            const char *_key;
        };


        class Label : public SingleValue<StorageType::LABEL> {
        public:
            using SingleValue::SingleValue;
            using SingleValue::getValue;
        };

        class LabelRaw : public SingleValue<StorageType::LABEL_RAW> {
        public:
            using SingleValue::SingleValue;
            using SingleValue::getValue;
        };

        class SuffixText : public SingleValue<StorageType::SUFFIX_TEXT> {
        public:
            using SingleValue::SingleValue;
            using SingleValue::getValue;
        };

        class SuffixHtml : public SingleValue<StorageType::SUFFIX_HTML> {
        public:
            using SingleValue::SingleValue;
            using SingleValue::getValue;
        };

        class Attribute : public KeyValuePair<StorageType::ATTRIBUTE> {
        public:
            using KeyValuePair::KeyValuePair;
            using KeyValuePair::getValue;
            using KeyValuePair::getKey;
        };

        class Option : public KeyValuePair<StorageType::OPTION> {
        public:
            using KeyValuePair::KeyValuePair;
            using KeyValuePair::getValue;
            using KeyValuePair::getKey;
        };

        class OptionNumKey : public SingleValue<StorageType::OPTION_NUM_KEY> {
        public:
            using SingleValue::getValue;

            OptionNumKey(int32_t key, const char *value) : SingleValue(value), _key(key) {}

            int32_t getKey() const {
                return _key;
            }

        private:
            int32_t _key;
        };

        class AttributeMinMax : public NoValue<StorageType::ATTRIBUTE_MIN_MAX> {
        public:
            AttributeMinMax(uint32_t __min, uint32_t __max) : _min(__min), _max(__max) {}
            AttributeMinMax(int32_t __min, int32_t __max) : _min(__min), _max(__max) {}

            int32_t getMin() const {
                return _min;
            }

            int32_t getMax() const {
                return _max;
            }

        private:
            int32_t _min;
            int32_t _max;
        };

        //TODO test disassembly and performance on ESP8266
        // requirement: TriviallyCopyable
        template<typename _Ta>
        void push_back(const _Ta &item) {
            size_t newSize = size() + sizeof(item) + sizeof(_Ta::type);
            size_t allocBlockSize = (newSize + 7) & ~7;                     // use mallocs 8 byte block size
            Vector::reserve(allocBlockSize);
#if 1
            auto oldSize = size();
            Vector::resize(newSize);
            auto dst = (uint8_t *)(data() + oldSize);
            *dst++ = static_cast<uint8_t>(_Ta::type);
            std::copy_n((const uint8_t *)(std::addressof(item)), sizeof(item), dst);
#elif 0
            Vector::push_back(static_cast<uint8_t>(_Ta::type));
            uint8_t count = sizeof(item);
            auto src = (const uint8_t *)(std::addressof(item));
            while (count--) {
                Vector::push_back(*src++);
            }
#elif 0
            Vector::push_back(static_cast<uint8_t>(_Ta::type));
            std::copy_n((const uint8_t *)(std::addressof(item)), sizeof(item), std::back_inserter(*this)); // using memcpy?
#endif
        }

        using CStrVector = std::vector<const char *>;

        void push_back(StorageType type, CStrVector::iterator begin, CStrVector::iterator end) {
            uint8_t count = (uint8_t)std::distance(begin, end);
            __LDBG_assert_printf(count <= PrintArgs::kMaximumPrintfArguments, "count=%u exceeds max=%u", count, PrintArgs::kMaximumPrintfArguments);
            size_t newSize = size() + (sizeof(*begin) * count) + sizeof(type);
            size_t allocBlockSize = (newSize + 7) & ~7;
            auto oldSize = size();
            Vector::reserve(allocBlockSize);
            Vector::resize(newSize);
            auto dst = (uint8_t *)(data() + oldSize);
            *dst++ = static_cast<uint8_t>(type) | ((count - 1) << 4);
            std::copy(reinterpret_cast<const uint8_t *>(&(*begin)), reinterpret_cast<const uint8_t *>(&(*(end - 1)) + 1), dst);
        }

        static Iterator print(Iterator iterator, Iterator end, PrintInterface &output) {
            if (iterator == end) {
                return end;
            }
            PrintValue::print(iterator, output);
            return iterator;
        }

        ConstIterator find(ConstIterator iterator, ConstIterator end, Pred pred) const;
    };

}
