/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "PrintString.h"
#include "StringKeyValueStore.h"

namespace KeyValueStorage {

    ItemLong *Item::_create(const String &name, int value)
    {
        return new ItemLong(name, value, TypeNameEnum::INT32_T);
    }

    ItemLong *Item::_create(const String &name, long value)
    {
        return new ItemLong(name, value);
    }

    ItemULong *Item::_create(const String &name, unsigned int value)
    {
        return new ItemULong(name, value, TypeNameEnum::UINT32_T);
    }

    ItemULong *Item::_create(const String &name, unsigned long value)
    {
        return new ItemULong(name, value);
    }

    ItemDouble *Item::_create(const String &name, float value)
    {
        return new ItemDouble(name, value, 6, TypeNameEnum::FLOAT);
    }

    ItemDouble *Item::_create(const String &name, double value)
    {
        return new ItemDouble(name, value, 6);
    }

    ItemDouble *Item::_create(const String &name, float value, uint8_t precision)
    {
        return new ItemDouble(name, value, precision, TypeNameEnum::FLOAT);
    }

    ItemDouble *Item::_create(const String &name, double value, uint8_t precision)
    {
        return new ItemDouble(name, value, precision);
    }

    ItemInt64 *Item::_create(const String &name, int64_t value)
    {
        return new ItemInt64(name, value);
    }

    ItemUInt64 *Item::_create(const String &name, uint64_t value)
    {
        return new ItemUInt64(name, value);
    }

    String Item::serialize() const
    {
        String str = _name;
        str += '=';
        _serializeData(str);
        return str;
    }

    void Item::_serializeData(String &str) const
    {
    }

    ItemULong::ItemULong(const String &name, unsigned long value, TypeNameEnum type) : Item(name, type), _value({ value })
    {
    }

    void ItemULong::_serializeData(String &str) const
    {
        str += PrintString(F("%lx"), _value.value);
    }

    ItemLong::ItemLong(const String &name, long value, TypeNameEnum type) : ItemULong(name, (unsigned long)value, type)
    {
    }

    void ItemLong::_serializeData(String &str) const
    {
        if (_value.is_signed) {
            str += PrintString(F("-%lx"), -_value.s_value);
        }
        else {
            ItemULong::_serializeData(str);
        }
    }

    ItemDouble::ItemDouble(const String &name, double value, uint8_t precision, TypeNameEnum type) : Item(name, type), _value(value), _precision(precision)
    {
    }

    void ItemDouble::_serializeData(String &str) const
    {
        str += PrintString(F("%.*f"), _precision, _value);
        // trim trailing zeros
        size_t len = str.length();
        while (len && str.charAt(len - 1) == '0' && str.charAt(len - 2) != '.') {
            len--;
        }
        str.remove(len, -1);
    }

    ItemUInt64::ItemUInt64(const String &name, uint64_t value, TypeNameEnum type) : Item(name, type), _value({ value })
    {
    }

    void ItemUInt64::_serializeData(String &str) const
    {
        _serializeTempValue(str, _value);
    }

    void ItemUInt64::_serializeTempValue(String &str, const decltype(_value) tmp) const
    {
        if (tmp.hi) {
            str += PrintString(F("%lx%08lx"), tmp.hi, tmp.lo);
        }
        else {
            str += PrintString(F("%lx"), tmp.lo);
        }
    }

    ItemInt64::ItemInt64(const String &name, int64_t value, TypeNameEnum type) : ItemUInt64(name, (uint64_t)value, type)
    {
    }

    void ItemInt64::_serializeData(String &str) const
    {
        if (_value.is_signed) {
            auto tmp = _value;
            tmp._s_value = -tmp._s_value;
            str += '-';
            _serializeTempValue(str, tmp);
        }
        else {
            _serializeTempValue(str, _value);
        }
    }


    Container::Container() {
    }

    Container::~Container() {
    }

    void Container::clear()
    {
        _items.clear();
    }

    bool Container::empty() const
    {
        return _items.empty();
    }

    size_t Container::size() const
    {
        return _items.size();
    }

    void Container::serialize(Print &output) const
    {
        int num = 0;
        for (auto &item : _items) {
            if (num++ != 0) {
                output.print(',');
            }
            output.print(item->serialize());
        }
    }

    bool Container::unserialize(const char *str)
    {
        bool modified = false;
        auto endPtr = strchr(str, ',');
        auto startPtr = str;
        do {
            String name;
            auto value = _getName(name, startPtr, endPtr);
            Item *item = _createFromValue(name, value, endPtr);
            if (item) {
                if (replace(item)) {
                    modified = true;
                }
            }
            if (endPtr) {
                startPtr = endPtr + 1;
                endPtr = strchr(startPtr, ',');
                if (!endPtr) {
                    value = _getName(name, startPtr, endPtr);
                    item = _createFromValue(name, value, endPtr);
                    if (item) {
                        if (replace(item)) {
                            modified = true;
                        }
                    }
                }
            }
        } while (endPtr);
        return modified;
    }

    const char *Container::_getName(String &name, const char *value, const char *end)
    {
        auto endName = strchr(value, '=');
        if (!endName || endName == value || (end && endName >= end)) { // no equal sign or name empty
            return nullptr;
        }
        if (*(endName + 1) == 0) {  // no value
            return nullptr;
        }
        auto ptr = value;
        while (isspace(*ptr) && ptr != endName) {
            ptr++;
        }
        if (ptr == endName) { // empty name
            return nullptr;
        }
        name = String();
#if 0 // use String.concat() if available
        name.concat(ptr, endName - ptr);
#else // append char by char
        while (ptr != endName) {
            name += *ptr++;
        }
#endif
        name.trim();
        if (name.length() == 0) { // empty name
            return nullptr;
        }
        return endName + 1;
    }

    Item *Container::_createFromValue(String &name, const char *value, const char *end)
    {
        while (isspace(*value) && *value && value != end) { //trim
            value++;
        }
        if (value == end || !*value) {
            return nullptr;
        }
        // look for "."
        bool isDouble = false;
        auto ptr = value;
        while (*ptr && ptr != end) {
            if (*ptr == '.') {
                isDouble = true;
                break;
            }
            ptr++;
        }
        if (isDouble) {
            return Item::_create(name, strtod(value, nullptr));
        }
        bool negative = false;
        if (*value == '-') {
            if (++value == end) {
                return nullptr;
            }
            negative = true;
        }
        size_t len = end ? end - value : strlen(value);
        if (len > 8) {
            auto val = strtoull(value, nullptr, 16);
            if (negative) {
                return Item::_create(name, -(long long)val);
            }
            else {
                return Item::_create(name, val);
            }
            //const char *loVal = value + len - 8;
            //char hiVal[9];
            //strncpy(hiVal, value, len - 8);
        }
        else {
            auto val = strtoul(value, nullptr, 16);
            if (negative) {
                return Item::_create(name, -(long)val);
            }
            else {
                return Item::_create(name, val);
            }
        }
    }

    void Container::merge(Container &container)
    {
        for (auto &item : container._items) {
            replace(item.release());
        }
        container._items.clear();
    }

    bool Container::add(Item *item)
    {
        remove(item->getName());
        _items.emplace_back(item);
        return true;
    }

    bool Container::replace(Item *item)
    {
        for (auto &_item : _items) {
            if (*_item == item->getName()) {
                _item.reset(item);
                return true;
            }
        }
        return add(item);
    }

    bool Container::remove(const Item &item)
    {
        return remove(item.getName());
    }

    bool Container::remove(const String &name)
    {
        auto iterator = std::remove_if(_items.begin(), _items.end(), [name](const ItemPtr &item) {
            return *item == name;
        });
        if (iterator == _items.end()) {
            return false;
        }
        _items.erase(iterator, _items.end());
        return true;
    }

    Container::ItemsVector::iterator Container::_find(const String &name)
    {
        return std::find_if(_items.begin(), _items.end(), [name](const ItemPtr &item) {
            return *item == name;
        });
    }

}
