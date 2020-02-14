/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <PrintArgs.h>

#ifndef DEBUG_BOOTSTRAP_MENU
#define DEBUG_BOOTSTRAP_MENU                    0
#endif

#include "push_pack.h"

class StaticString {
public:

    StaticString(StaticString &&str) noexcept {
        *this = std::move(str);
    }
    StaticString(const String &str) : StaticString(str.c_str(), (uint16_t)str.length()) {
    }
    StaticString(const char *str) : StaticString(str, (uint16_t)strlen(str)) {
    }
    ~StaticString() {
        __free();
    }

    String toString() const {
        return _ptr ? (isProgMem() ? String(FPSTR(_ptr)) : String(reinterpret_cast<const char *>(_ptr))) : emptyString;
    }

    const char *c_str() const {
        return reinterpret_cast<const char *>(_ptr);
    }

    PGM_P p_str() const {
        return reinterpret_cast<PGM_P>(_ptr);
    }

    operator bool() const {
        return _ptr != nullptr;
    }

    bool equalsIgnoreCase(const String &str) const {
        if (_ptr) {
            if (isProgMem()) {
                return !strcasecmp_P(str.c_str(), p_str());
            }
            else {
                return str.equalsIgnoreCase(c_str());
            }
        }
        return false;
    }

    bool equalsIgnoreCase(const StaticString &str) const {
        if (_ptr) {
            if (isProgMem() && str.isProgMem()) {
                return !strcasecmp_P_P(p_str(), str.p_str());
            }
            else if (isProgMem()) {
                return !strcasecmp_P(str.c_str(), p_str());
            }
            else if (str.isProgMem()) {
                return !strcasecmp_P(c_str(), str.p_str());
            }
        }
        return false;
    }

    void __free() {
        if (!isProgMem() && _ptr) {
            free(_ptr);
            _ptr = nullptr;
        }
        new(this) StaticString();
    }

#if ESP8266

    StaticString() : _ptr() {
    }

    StaticString(const char *str, uint16_t length) : _ptr(length ? malloc(length + 1) : nullptr) {
        if (_ptr) {
            memcpy(_ptr, str, length + 1);
        }
    }

    StaticString(const __FlashStringHelper *str) : _ptr(const_cast<__FlashStringHelper *>(str)) {
    }

    StaticString &operator=(StaticString &&str) {
        _ptr = str._ptr;
        str._ptr = nullptr;
        return *this;
    }

    bool isProgMem() const {
        return (((uint32_t)_ptr) >= 0x40200000);
    }

private:
    void *_ptr;
#else

    StaticString() : _ptr(), _length(), _progmem() {
    }

    StaticString &operator=(StaticString &&str) noexcept {
        _ptr = str._ptr;
        _length = str._length;
        _progmem = str._progmem;
        str._ptr = nullptr;
        str._length = 0;
        str._progmem = 0;
        return *this;
    }

    StaticString(const char *str, uint16_t length) : _ptr(length ? malloc(length + 1) : nullptr), _length(length), _progmem(false) {
        if (_ptr) {
            memcpy(_ptr, str, length + 1);
        }
    }

    StaticString(const __FlashStringHelper *str) : _ptr(const_cast<__FlashStringHelper *>(str)), _length((uint16_t)strlen_P(reinterpret_cast<PGM_P>(str))), _progmem(true) {
    }

    bool isProgMem() const {
        return _progmem;
    }

private:
    void *_ptr;
    uint16_t _length : 15;
    uint16_t _progmem : 1;
#endif
};

// 11 byte RAM usage per menu entry when using PROGMEM strings

class BootstrapMenu {
public:
    using PrintInterface = PrintArgs::PrintInterface;

    typedef uint8_t menu_item_id_t;
    typedef uint8_t menu_item_parent_id_t;

    static const uint8_t InvalidMenuId = ~0;

    class Item;

    class FindHelper {
    public:
        struct LabelType {};
        struct UriType {};
        struct MenuIdType {};
        struct ParentMenuIdType {};

        FindHelper(menu_item_id_t menuId, MenuIdType) : _menuId(menuId), _parentMenuId(InvalidMenuId) {
        }
        FindHelper(menu_item_parent_id_t parentMenuId, ParentMenuIdType) : _menuId(InvalidMenuId), _parentMenuId(parentMenuId) {
        }
        FindHelper(const String &uri, menu_item_parent_id_t parentMenuId, UriType) : _uri(uri), _menuId(InvalidMenuId), _parentMenuId(parentMenuId) {
        }
        FindHelper(const String &label, menu_item_parent_id_t parentMenuId, LabelType) : _label(label), _menuId(InvalidMenuId), _parentMenuId(parentMenuId) {
        }

    private:
        friend Item;

        String _uri;
        String _label;
        menu_item_id_t _menuId;
        menu_item_parent_id_t _parentMenuId;
    };

    class Item {
    public:
        Item() : _label(), _uri(), _id(InvalidMenuId), _parentMenuId(InvalidMenuId) {
        }
        Item(BootstrapMenu &menu, StaticString &&label, StaticString &&uri, menu_item_parent_id_t parentMenuId = InvalidMenuId) :
            _label(std::move(label)), _uri(std::move(uri)), _id(menu._getUnqiueId()), _parentMenuId(parentMenuId) {
        }

        bool operator ==(const FindHelper &helper) const {
            return
                (helper._menuId == InvalidMenuId || helper._menuId == _id) &&
                (helper._parentMenuId == InvalidMenuId || helper._parentMenuId == _parentMenuId) &&
                (helper._label.length() == 0 || _label.equalsIgnoreCase(helper._label)) &&
                (helper._uri.length() == 0 || _uri.equalsIgnoreCase(helper._uri));
        }

        bool hasLabel() const {
            return _label != false;
        }

        bool hashURI() const {
            return _uri;
        }

        void setLabel(const String &label) {
            _label = label;
        }

        void setUri(const String &uri) {
            _uri = uri;
        }

        const StaticString &getLabel() const {
            return _label;
        }

        const StaticString &getUri() const {
            return _uri;
        }

        void setParentMenuId(menu_item_parent_id_t menuId) {
            _parentMenuId = menuId;
        }

        menu_item_parent_id_t getParentMenuId() const {
            return _parentMenuId;
        }

        menu_item_id_t getId() const {
            return _id;
        }

    private:
        StaticString _label;
        StaticString _uri;
        struct __attribute__packed__ {
            menu_item_id_t _id;
            menu_item_parent_id_t _parentMenuId;
        };
    };

    typedef std::vector<Item> ItemsVector;
    typedef std::vector<Item>::iterator ItemsVectorIterator;

public:
    BootstrapMenu();
    ~BootstrapMenu();

    bool isValid(ItemsVectorIterator iterator) const {
        return iterator != _items.end();
    }

    menu_item_id_t getId(ItemsVectorIterator iterator) const {
        if (isValid(iterator)) {
            return iterator->getId();
        }
        return InvalidMenuId;
    }

    menu_item_parent_id_t addMenu(StaticString &&label, menu_item_id_t afterId = menu_item_id_t());
    menu_item_id_t addSubMenu(StaticString &&label, StaticString &&uri, menu_item_parent_id_t parentMenuId, menu_item_id_t afterId = menu_item_id_t());

    ItemsVectorIterator findMenuByLabel(const String &label, menu_item_parent_id_t menuId = InvalidMenuId);
    ItemsVectorIterator findMenuByURI(const String &uri, menu_item_parent_id_t menuId = InvalidMenuId);

    // get number of menu items
    //menu_item_id_t getItemCount(menu_item_id_t menuId) const;

    inline ItemsVectorIterator getItem(menu_item_id_t menuId) {
        return std::find(_items.begin(), _items.end(), FindHelper(menuId, FindHelper::MenuIdType()));
    }

    // create menu item
    void html(PrintInterface &output, ItemsVectorIterator top);

    // create main menu
    void html(PrintInterface &output);

    // create sub menu
    void htmlSubMenu(PrintInterface &output, ItemsVectorIterator top, uint8_t active);

    // public:
    //     void createCache();
    // private:
    //     String _cacheFilename;

private:
    friend Item;

    menu_item_id_t _add(Item &item, menu_item_id_t afterId);
    menu_item_id_t _add(Item &&item, menu_item_id_t afterId);
    menu_item_id_t _getUnqiueId();

private:
    ItemsVector _items;
    menu_item_id_t _unqiueId;
};

#include "pop_pack.h"
