/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <TypeName.h>

namespace KeyValueStorage {

    class ItemLong;
    class ItemULong;
    class ItemDouble;
    class ItemInt64;
    class ItemUInt64;
    class Container;

    class Item {
    public:
        Item() : _type(TypeNameEnum::INVALID) {
        }
        Item(const String &name, TypeNameEnum type) : _name(name), _type(type) {
        }
        virtual ~Item() {
        }

        operator bool() const {
            return _type != TypeNameEnum::INVALID;
        }

        bool operator==(const char *str) const {
            return _name.equals(str);
        }
        bool operator==(const String &str) const {
            return _name.equals(str);
        }
        bool operator==(const __FlashStringHelper *str) const {
            return _name.equals(str);
        }

        const String &getName() const {
            return _name;
        }

#if TYPENAME_HAVE_PROGMEM_NAME
        const __FlashStringHelper *getTypeStr() {
            return TypeName_getName(_type);
        }
#endif

        static ItemLong *_create(const String &name, int value);
        static ItemLong *_create(const String &name, long value);
        static ItemULong *_create(const String &name, unsigned int value);
        static ItemULong *_create(const String &name, unsigned long value);
        static ItemDouble *_create(const String &name, float value);
        static ItemDouble *_create(const String &name, double value);
        static ItemDouble *_create(const String &name, float value, uint8_t precision);
        static ItemDouble *_create(const String &name, double value, uint8_t precision);
        static ItemInt64 *_create(const String &name, int64_t value);
        static ItemUInt64 *_create(const String &name, uint64_t value);

        template<class T, class ReturnType =
            typename std::conditional<
            std::is_floating_point<T>::value,
            ItemDouble,
            typename std::conditional<
            std::is_same<T, int64_t>::value,
            ItemInt64,
            typename std::conditional<
            std::is_same<T, uint64_t>::value,
            ItemUInt64,
            typename std::conditional<
            std::is_signed<T>::value,
            ItemLong,
            ItemULong
            >::type
            >::type
            >::type
            >::type
        >
            static Item *create(const String &name, T value) {
            ReturnType *item = _create(name, value);
            item->_type = constexpr_TypeName<T>::id();
            return item;
        }

        String serialize() const;

    protected:
        virtual void _serializeData(String &str) const;

    protected:
        String _name;
        TypeNameEnum _type;
    };

    class ItemULong : public Item {
    public:
        ItemULong(const String &name, unsigned long value, TypeNameEnum type = TypeNameEnum::ULONG);

    protected:
        virtual void _serializeData(String &str) const;

    protected:
        union {
            unsigned long value;
            long s_value;
            struct __attribute__packed__ {
                unsigned long _dummy : 31;
                unsigned long is_signed : 1;
            };
        } _value;
    };

    class ItemLong : public ItemULong {
    public:
        ItemLong(const String &name, long value, TypeNameEnum type = TypeNameEnum::LONG);

    protected:
        virtual void _serializeData(String &str) const;
    };

    class ItemDouble : public Item {
    public:
        ItemDouble(const String &name, double value, uint8_t precision = 6, TypeNameEnum type = TypeNameEnum::DOUBLE);

    protected:
        virtual void _serializeData(String &str) const;

    private:
        double _value;
        uint8_t _precision;
    };

    class ItemUInt64 : public Item {
    public:
        ItemUInt64(const String &name, uint64_t value, TypeNameEnum type = TypeNameEnum::UINT64_T);

    protected:
        union {
            uint64_t _value;
            int64_t _s_value;
            struct __attribute__packed__ {
                uint32_t lo;
                union {
                    uint32_t hi;
                    struct {
                        uint32_t _dummy : 31;
                        uint32_t is_signed : 1;
                    };
                };
            };
        } _value;

    protected:
        virtual void _serializeData(String &str) const;
        void _serializeTempValue(String &str, const decltype(_value) tmp) const;
    };

    class ItemInt64 : public ItemUInt64 {
    public:
        ItemInt64(const String &name, int64_t value, TypeNameEnum type = TypeNameEnum::INT64_T);

    protected:
        virtual void _serializeData(String &str) const;
    };

    using ContainerPtr = std::shared_ptr<Container>;

    class Container {
    public:
        using ItemPtr = std::unique_ptr<Item>;
        using ItemsVector = std::vector<ItemPtr>;

        Container();
        ~Container();

        void clear();
        bool empty() const;
        size_t size() const;

        void serialize(Print &output) const;
        bool unserialize(const char *str);

        // add/replace all items from container
        void merge(Container &container);

        bool add(Item *item);
        bool replace(Item *item);
        bool remove(const Item &item);
        bool remove(const String &name);

    private:
        const char *_getName(String &name, const char *value, const char *end);
        Item *_createFromValue(String &name, const char *value, const char *end);
        ItemsVector::iterator _find(const String &name);

    private:
        ItemsVector _items;
    };

}
