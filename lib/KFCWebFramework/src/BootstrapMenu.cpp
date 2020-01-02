/**
* Author: sascha_lammers@gmx.de
*/

#include "BootstrapMenu.h"
#include <PrintHtmlEntitiesString.h>
#include <sha1.h>

#if DEBUG_BOOTSTRAP_MENU
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

BootstrapMenu::BootstrapMenu() : _unqiueId(0)
{
}

BootstrapMenu::~BootstrapMenu()
{
	for (auto &item : _items) {
		item._destroy();
	}
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addMenu(const __FlashStringHelper* label, menu_item_id_t afterId)
{
	Item item(*this);
	item.setLabel(label);
	return _add(item, afterId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addMenu(const String& label, menu_item_id_t afterId)
{
	Item item(*this);
	item.setLabel(label);
	return _add(item, afterId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(const __FlashStringHelper* label, const __FlashStringHelper* uri, menu_item_id_t parentMenuId, menu_item_id_t afterId)
{
	Item item(*this);
	item.setLabel(label);
	item.setURI(uri);
	item.setParentMenuId(parentMenuId);
	return _add(item, afterId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(const String& label, const __FlashStringHelper* uri, menu_item_id_t parentMenuId, menu_item_id_t afterId)
{
	Item item(*this);
	item.setLabel(label);
	item.setURI(uri);
	item.setParentMenuId(parentMenuId);
	return _add(item, afterId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(const __FlashStringHelper* label, const String& uri, menu_item_id_t parentMenuId, menu_item_id_t afterId)
{
	Item item(*this);
	item.setLabel(label);
	item.setURI(uri);
	item.setParentMenuId(parentMenuId);
	return _add(item, afterId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(const String& label, const String& uri, menu_item_id_t parentMenuId, menu_item_id_t afterId)
{
	Item item(*this);
	item.setLabel(label);
	item.setURI(uri);
	item.setParentMenuId(parentMenuId);
	return _add(item, afterId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::findMenuByLabel(const String &label, menu_item_id_t menuId) const
{
	for (const auto &item: _items) {
		if (item.getParentMenuId() == menuId && item.getLabel().equalsIgnoreCase(label)) {
			return item.getId();
		}
	}
	return INVALID_ID;
}

BootstrapMenu::menu_item_id_t BootstrapMenu::findMenuByURI(const String &uri, menu_item_id_t menuId) const
{
	for (const auto &item: _items) {
		if (item.getParentMenuId() == menuId && item.getURI().equalsIgnoreCase(uri)) {
			return item.getId();
		}
	}
	return INVALID_ID;
}

BootstrapMenu::menu_item_id_t BootstrapMenu::getItemCount(menu_item_id_t menuId) const
{
	menu_item_id_t count = 0;
	for (const auto &item : _items) {
		if (item.getParentMenuId() == menuId) {
			count++;
		}
	}
	_debug_printf_P(PSTR("BootstrapMenu::getItemCount(menuId=%d): count=%d\n"), menuId, count);
	return count;
}

void BootstrapMenu::html(Print &output, menu_item_id_t menuId, bool dropDown)
{
	_debug_printf_P(PSTR("BootstrapMenu::html(menuId=%d, dropDown=%d)\n"), menuId, dropDown);

	auto topMenuIterator = getItem(menuId);
	if (topMenuIterator == _items.end()) {
		return;
	}
	auto& topMenu = *topMenuIterator;
	auto subItems = getItemCount(menuId);

	if (dropDown) {
		output.printf_P(PSTR("<a class=\"dropdown-item\" href=\"%s\">%s</a>"), topMenu.getURI().c_str(), topMenu.getLabel().c_str());
	}
	else if (topMenu.hashURI()) { // menu bar only
		output.printf_P(PSTR("<li class=\"nav-item\"><a class=\"nav-link\" href=\"%s\">%s</a></li>"), topMenu.getURI().c_str(), topMenu.getLabel().c_str());
	}
	else if (subItems) { // drop down empty?
		output.printf_P(("<li class=\"nav-item dropdown\"><a class=\"nav-link dropdown-toggle\" href=\"#\" id=\"navbarDropdown%u\" role=\"button\" data-toggle=\"dropdown\" aria-haspopup=\"true\" aria-expanded=\"false\">%s</a>"), menuId, topMenu.getLabel().c_str());
		output.print(F("<div class=\"dropdown-menu\" aria-labelledby=\"navbarDropdownConfig\">"));
		for (const auto &item: _items) {
			if (item.getParentMenuId() == menuId) {
				html(output, item.getId(), true);
			}
		}
		output.print(F("</div></li>"));
	}
}

// void BootstrapMenu::createCache()
// {
// 	PrintString output;
// 	SHA1 sha1;
// 	uint8_t hash[sha1.hashSize()];
// 	html(output);
// 	sha1.update(output.c_str(), output.length());
// 	sha1.finalize(hash, sizeof(hash));

// 	_cacheFilename = PrintString(F("/c/menu%02x%02x%02x%02x%02x%02x"), hash[0], hash[1], hash[2], hash[3], hash[4], hash[5]);

// 	if (!SPIFFS.exists(_cacheFilename)) {
// 		File file = SPIFFS.open(_cacheFilename, fs::FileOpenMode::write);
// 		if (file) {
// 			file.write(output.c_str(), output.length());
// 			file.close();
// 		}
// 	}
// }

void BootstrapMenu::html(Print& output)
{
	_debug_printf_P(PSTR("BootstrapMenu::html()\n"));
	for (const auto &item: _items) {
		if (item.getParentMenuId() == INVALID_ID) {
			html(output, item.getId(), false);
		}
	}
}

void BootstrapMenu::htmlSubMenu(Print& output, menu_item_id_t menuId, uint8_t active)
{
	menu_item_id_t pos = 0;
	for (const auto &item : _items) {
		if (item.getParentMenuId() == menuId) {
			output.printf_P(PSTR("<a href=\"%s\" class=\"list-group-item list-group-item-action align-items-start%s\"><h5 class=\"mb-1\">%s</h5></a>"),
				item.getURI().c_str(),
				active == pos ? PSTR(" active") : _sharedEmptyString.c_str(),
				item.getLabel().c_str()
			);
			pos++;
		}
	}
	_debug_printf_P(PSTR("BootstrapMenu::htmlSubMenu(menuId=%d, active=%d): items=%d\n"), menuId, active, pos);
}

BootstrapMenu::ItemsVectorIterator BootstrapMenu::getItem(menu_item_id_t menuId)
{
	for (auto item = _items.begin(); item != _items.end(); ++item) {
		if (item->getId() == menuId) {
			return item;
		}
	}
	return _items.end();
}

BootstrapMenu::menu_item_id_t BootstrapMenu::_add(Item &item, menu_item_id_t afterId)
{
	auto prev = getItem(afterId);
	if (prev == _items.end()) {
		_items.emplace_back(std::move(item));
		return _items.back().getId();
	}
	++prev;
	auto newItem = _items.emplace(prev, std::move(item));
	return newItem->getId();
}

BootstrapMenu::menu_item_id_t BootstrapMenu::_getUnqiueId()
{
	return ++_unqiueId;
}
