/**
* Author: sascha_lammers@gmx.de
*/

#include "BootstrapMenu.h"

#if DEBUG_BOOTSTRAP_MENU
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

BootstrapMenu::BootstrapMenu()
{
}

BootstrapMenu::~BootstrapMenu()
{
	for (auto& item : _items) {
		item._destroy();
	}
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addMenu(const __FlashStringHelper* label)
{
	MenuItem item;
	item.setLabel(label);
	return _add(item);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addMenu(const String& label)
{
	MenuItem item;
	item.setLabel(label);
	return _add(item);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(const __FlashStringHelper* label, const __FlashStringHelper* uri, menu_item_id_t parentMenuId)
{
	MenuItem item;
	item.setLabel(label);
	item.setURI(uri);
	item.setParentMenuId(parentMenuId);
	return _add(item);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(const String& label, const __FlashStringHelper* uri, menu_item_id_t parentMenuId)
{
	MenuItem item;
	item.setLabel(label);
	item.setURI(uri);
	item.setParentMenuId(parentMenuId);
	return _add(item);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(const __FlashStringHelper* label, const String& uri, menu_item_id_t parentMenuId)
{
	MenuItem item;
	item.setLabel(label);
	item.setURI(uri);
	item.setParentMenuId(parentMenuId);
	return _add(item);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(const String& label, const String& uri, menu_item_id_t parentMenuId)
{
	MenuItem item;
	item.setLabel(label);
	item.setURI(uri);
	item.setParentMenuId(parentMenuId);
	return _add(item);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::getMenu(const String &label, menu_item_id_t menuId) const
{
	menu_item_id_t num = 0;
	for (const auto &item: _items) {
		if (item.getParentMenuId() == menuId && item.getLabel().equalsIgnoreCase(label)) {
			_debug_printf_P(PSTR("BootstrapMenu::getMenu(%s, %d) = %d\n"), label.c_str(), menuId, num);
			return num;
		}
		num++;
	}
	_debug_printf_P(PSTR("BootstrapMenu::getMenu(%s, %d) = %d\n"), label.c_str(), menuId, INVALID_ID);
	return INVALID_ID;
}

BootstrapMenu::menu_item_id_t BootstrapMenu::getItemCount(menu_item_id_t menuId) const
{
	menu_item_id_t count = 0;
	for (const auto& item : _items) {
		if (item.getParentMenuId() == menuId) {
			count++;
		}
	}
	_debug_printf_P(PSTR("BootstrapMenu::getItemCount(menuId=%d): count=%d\n"), menuId, count);
	return count;
}

void BootstrapMenu::html(Print &output, menu_item_id_t menuId, bool dropDown) const
{
	_debug_printf_P(PSTR("BootstrapMenu::html(menuId=%d, dropDown=%d)\n"), menuId, dropDown);

	auto& topMenu = _items.at(menuId);
	auto subItems = getItemCount(menuId);

	if (dropDown) {
		output.printf_P(PSTR("<a class=\"dropdown-item\" href=\"%s\">%s</a>\n"), topMenu.getURI().c_str(), topMenu.getLabel().c_str());
	}
	else if (topMenu.hashURI()) { // menu bar only
		output.printf_P(PSTR("<li class=\"nav-item\"><a class=\"nav-link\" href=\"%s\">%s</a></li>\n"), topMenu.getURI().c_str(), topMenu.getLabel().c_str());
	}
	else if (subItems) { // drop down empty?
		output.printf_P(("<li class=\"nav-item dropdown\">\n<a class=\"nav-link dropdown-toggle\" href=\"#\" id=\"navbarDropdown%u\" role=\"button\" data-toggle=\"dropdown\" aria-haspopup=\"true\" aria-expanded=\"false\">%s</a>"), menuId, topMenu.getLabel().c_str());
		output.print(F("<div class=\"dropdown-menu\" aria-labelledby=\"navbarDropdownConfig\">\n"));
		menu_item_id_t num = 0;
		for (const auto &item: _items) {
			if (item.getParentMenuId() == menuId) {
				html(output, num, true);
			}
			num++;
		}
		output.print(F("</div></li>\n"));
	}
}

void BootstrapMenu::html(Print& output) const
{
	_debug_printf_P(PSTR("BootstrapMenu::html()\n"));
	menu_item_id_t num = 0;
	for (const auto& item: _items) {
		if (item.getParentMenuId() == INVALID_ID) {
			html(output, num, false);
		}
		num++;
	}
}

void BootstrapMenu::htmlSubMenu(Print& output, menu_item_id_t menuId, uint8_t active) const
{
	menu_item_id_t num = 0, pos = 0;
	for (const auto& item : _items) {
		if (item.getParentMenuId() == menuId) {
			output.printf_P(PSTR("<a href=\"%s\" class=\"list-group-item list-group-item-action align-items-start%s\"><h5 class=\"mb-1\">%s</h5></a>"), 
				item.getURI().c_str(), 
				active == pos ? PSTR(" active") : _sharedEmptyString.c_str(),
				item.getLabel().c_str()
			);
			pos++;
		}
		num++;
	}
	_debug_printf_P(PSTR("BootstrapMenu::htmlSubMenu(menuId=%d, active=%d): items=%d\n"), menuId, active, pos);
}

BootstrapMenu::MenuItem& BootstrapMenu::getItem(menu_item_id_t menuId)
{
	return _items.at(menuId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::_add(MenuItem &item)
{
	_items.emplace_back(std::move(item));
	return (menu_item_id_t)_items.size() - 1;
}
