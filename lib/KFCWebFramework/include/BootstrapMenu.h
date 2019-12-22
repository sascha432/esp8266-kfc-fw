/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>

class BootstrapMenu {
public:
    typedef struct {
        uint8_t label_progmem : 1;
        uint8_t URI_progmem : 1;
    } MenuItemFlags_t;

    typedef uint8_t menu_item_id_t;

    class MenuItem {
    public:
        MenuItem() : _label(nullptr), _URI(nullptr), _parentMenuId(INVALID_ID) {
            memset(&_flags, 0, sizeof(_flags));
        }
        ~MenuItem() {
            clearLabel();
            clearURI();
        }

        void clearLabel() {
            if (_flags.label_progmem == 0 && _label) {
                delete reinterpret_cast<String *>(_label);
            }
            _label = nullptr;
        }
        void clearURI() {
            if (_flags.URI_progmem == 0 && _URI) {
                delete reinterpret_cast<String *>(_URI);
            }
            _URI = nullptr;
        }

        void setLabel(const String &label) {
            clearLabel();
            _label = reinterpret_cast<void *>(new String(label));
            _flags.label_progmem = 0;
        }

        void setLabel(const __FlashStringHelper *label) {
            clearLabel();
            _label = (void *)label;
            _flags.label_progmem = 1;
        }

        bool hasLabel() const {
            return _label != nullptr;
        }

        void setURI(const __FlashStringHelper *uri) {
            clearURI();
            _URI = (void *)uri;
            _flags.URI_progmem = 1;
        }

        void setURI(const String &uri) {
            clearURI();
            _URI = reinterpret_cast<void *>(new String(uri));
            _flags.URI_progmem = 0;
        }

        bool hashURI() const {
            return _URI != nullptr;
        }

        String getLabel() const {
            if (_label) {
                if (_flags.label_progmem) {
                    return String(reinterpret_cast<const __FlashStringHelper*>(_label));
                }
                else {
                    return *reinterpret_cast<String *>(_label);
                }
            }
            return String();
        }

        String getURI() const {
            if (_URI) {
                if (_flags.URI_progmem) {
                    return String(reinterpret_cast<const __FlashStringHelper *>(_URI));
                }
                else {
                    return *reinterpret_cast<String *>(_URI);
                }
            }
            return String();
        }

        void setParentMenuId(menu_item_id_t menuId) {
            _parentMenuId = menuId;
        }

        uint16_t getParentMenuId() const {
            return _parentMenuId;
        }

    private:
        void *_label;
        void *_URI;
        MenuItemFlags_t _flags;
        menu_item_id_t _parentMenuId;
    };

    typedef std::vector<MenuItem> ItemsVector;

    static const menu_item_id_t INVALID_ID = ~0;

public:
    BootstrapMenu();

    menu_item_id_t addMenu(const __FlashStringHelper* label);
    menu_item_id_t addMenu(const String &label);
    menu_item_id_t addSubMenu(const __FlashStringHelper *label, const __FlashStringHelper *uri, menu_item_id_t parentMenuId);
    menu_item_id_t addSubMenu(const String& label, const __FlashStringHelper *uri, menu_item_id_t parentMenuId);
    menu_item_id_t addSubMenu(const __FlashStringHelper *label, const String &uri, menu_item_id_t parentMenuId);
    menu_item_id_t addSubMenu(const String &label, const String &uri, menu_item_id_t parentMenuId);

    // get menu by label
    menu_item_id_t getMenu(const String& label, menu_item_id_t menuId = 0) const;
    // get number of menu items
    menu_item_id_t getItemCount(menu_item_id_t menuId) const;

    // create menu item
    void html(Print& output, menu_item_id_t menuId) const;
    // create main menu
    void html(Print& output) const;

    // create sub menu
    void htmlSubMenu(Print& output, menu_item_id_t menuId, uint8_t active) const;

    // get menu item by id
    MenuItem& getItem(menu_item_id_t menuId);

private:
    menu_item_id_t _add(const MenuItem &item);

private:
    ItemsVector _items;
};
