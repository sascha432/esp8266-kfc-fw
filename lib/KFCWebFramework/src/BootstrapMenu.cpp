/**
* Author: sascha_lammers@gmx.de
*/

#include "BootstrapMenu.h"

BootstrapMenu::BootstrapMenu()
{
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
			return num;
		}
		num++;
	}
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
	return count;
}

void BootstrapMenu::html(Print &output, menu_item_id_t menuId) const
{
	auto& topMenu = _items.at(menuId);
	if (topMenu.hashURI()) {
		output.printf_P(PSTR("<li class=\"nav-item dropdown\"><a class=\"dropdown-item\" href=\"%s\">%s</a></li>"), topMenu.getURI().c_str(), topMenu.getLabel().c_str());
	}
	else if (getItemCount(menuId)) { // drop down empty?
		output.printf_P(("<li class=\"nav-item dropdown\"><a class=\"nav-link dropdown-toggle\" href=\"#\" id=\"navbarDropdown%u\" role=\"button\" data-toggle=\"dropdown\" aria-haspopup=\"true\" aria-expanded=\"false\">%s</a>"), menuId, topMenu.getLabel().c_str());
		output.print(F("<div class=\"dropdown-menu\" aria-labelledby=\"navbarDropdownConfig\">"));
		menu_item_id_t num = 0;
		for (const auto &item: _items) {
			if (item.getParentMenuId() == menuId) {
				html(output, num);
			}
			num++;
		}
		output.print(F("</div></li>"));
	}
}

void BootstrapMenu::html(Print& output) const
{
	menu_item_id_t num = 0;
	for (const auto& item: _items) {
		if (!item.getParentMenuId()) {
			html(output, num);
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
}

BootstrapMenu::MenuItem& BootstrapMenu::getItem(menu_item_id_t menuId)
{
	return _items.at(menuId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::_add(const MenuItem &item)
{
	_items.push_back(item);
	return (menu_item_id_t)_items.size() - 1;
}
