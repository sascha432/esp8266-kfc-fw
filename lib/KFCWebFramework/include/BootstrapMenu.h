/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <PrintArgs.h>
#include <StaticString.h>
#include <MicrosTimer.h>

#ifndef DEBUG_BOOTSTRAP_MENU
#define DEBUG_BOOTSTRAP_MENU                    0
#endif

#ifndef DEBUG_BOOTSTRAP_MENU_RENDER_TIME
#define DEBUG_BOOTSTRAP_MENU_RENDER_TIME        0 //DEBUG_BOOTSTRAP_MENU
#endif

 #include "push_pack.h"

// typedef struct __attribute__packed__ {
//     PGM_P _label;
//     PGM_P _uri;
//     uint8_t _id;
//     uint8_t _parent_id;
// } BoostrapMenuItem_t;

// extern BoostrapMenuItem_t bootstrap_items[] PROGMEM;

class BootstrapMenu {
public:
    using PrintInterface = PrintArgs::PrintInterface;

    typedef uint8_t menu_item_id_t;
    typedef uint8_t menu_item_parent_id_t;

    class MenuItem {
    public:

        MenuItem(uint8_t id, BootstrapMenu &menu) : _id(id), _menu(menu) {}

        operator menu_item_id_t() const {
            return _id;
        }

        MenuItem addSubMenu(StaticString &&label, menu_item_id_t afterId = menu_item_id_t()) {
            return MenuItem(_menu.addSubMenu(std::move(label), _id, afterId), _menu);
        }

        MenuItem addMenuItem(StaticString &&label, StaticString &&uri, menu_item_id_t afterId = menu_item_id_t()) {
            return MenuItem(_menu.addMenuItem(std::move(label), std::move(uri), _id, afterId), _menu);
        }

        MenuItem addDivider(menu_item_id_t afterId = menu_item_id_t()) {
            return MenuItem(_menu.addDivider(_id, afterId), _menu);
        }

    private:
        menu_item_id_t _id;
        BootstrapMenu &_menu;
    };

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

        bool hasURI() const {
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
    BootstrapMenu() : _unqiueId(0) {}
    ~BootstrapMenu() {}

    inline bool isValid(ItemsVectorIterator iterator) const {
        return iterator != _items.end();
    }

    inline menu_item_id_t getId(ItemsVectorIterator iterator) const {
        if (isValid(iterator)) {
            return iterator->getId();
        }
        return InvalidMenuId;
    }

    inline void remove(ItemsVectorIterator iterator) {
        _items.erase(iterator);
    }

    MenuItem getMenuItem(menu_item_id_t id) {
        return MenuItem(id, *this);
    }

    MenuItem addMenu(StaticString &&label, menu_item_id_t afterId = menu_item_id_t()) {
	    return MenuItem(_add(std::move(Item(*this, std::move(label), std::move(StaticString()))), afterId), *this);
    }

    MenuItem addSubMenu(StaticString &&label, menu_item_parent_id_t parentMenuId, menu_item_id_t afterId = menu_item_id_t()) {
	    return MenuItem(addMenuItem(std::move(label), std::move(StaticString()), parentMenuId, afterId), *this);
    }

    MenuItem addMenuItem(StaticString &&label, StaticString &&uri, menu_item_parent_id_t parentMenuId, menu_item_id_t afterId = menu_item_id_t()) {
	    return MenuItem(_add(std::move(Item(*this, std::move(label), std::move(uri), parentMenuId)), afterId), *this);
    }

    MenuItem addDivider(menu_item_parent_id_t parentMenuId, menu_item_id_t afterId = menu_item_id_t()) {
	    return MenuItem(_add(std::move(Item(*this, std::move(StaticString()), std::move(StaticString()), parentMenuId)), afterId), *this);
    }

    ItemsVectorIterator findMenuByLabel(const String &label, menu_item_parent_id_t menuId = InvalidMenuId) {
	    return std::find(_items.begin(), _items.end(), FindHelper(label, menuId, FindHelper::LabelType()));
    }
    ItemsVectorIterator findMenuByURI(const String &uri, menu_item_parent_id_t menuId = InvalidMenuId) {
    	return std::find(_items.begin(), _items.end(), FindHelper(uri, menuId, FindHelper::UriType()));
    }

    // get number of menu items
    //menu_item_id_t getItemCount(menu_item_id_t menuId) const;

    inline ItemsVectorIterator getItem(menu_item_id_t menuId) {
        return std::find(_items.begin(), _items.end(), FindHelper(menuId, FindHelper::MenuIdType()));
    }

    // create menu item
    void html(PrintInterface &output, ItemsVectorIterator top, uint8_t level = 0);

    // create main menu
    void html(PrintInterface &output);

    // create sub menu
    void htmlSubMenu(PrintInterface &output, ItemsVectorIterator top, uint8_t active);

private:
    friend Item;

    // menu_item_id_t _add(const Item &item, menu_item_id_t afterId);
    menu_item_id_t _add(Item &&item, menu_item_id_t afterId);
    menu_item_id_t _getUnqiueId();

private:
    ItemsVector _items;
    menu_item_id_t _unqiueId;
};

#include "pop_pack.h"
