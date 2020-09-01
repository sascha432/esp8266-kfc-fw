/**
* Author: sascha_lammers@gmx.de
*/

#include "BootstrapMenu.h"
#include <PrintHtmlEntitiesString.h>
#include <SHA1.h>

#if _WIN32
#define BOOTSTRAP_MENU_CRLF "\n"
#else
#define BOOTSTRAP_MENU_CRLF ""
#endif

#if DEBUG_BOOTSTRAP_MENU
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// BoostrapMenuItem_t bootstrap_items[] PROGMEM = {
// 	{ SPGM(Home), SPGM(index_html), 1, 0, },
// 	{ SPGM(Status), SPGM(status_html), 2, 0, },
// 	{ SPGM(Configuration), nullptr, 3, 0 },
// 	{ SPGM(Device), nullptr, 4, 0 },
// 	{ SPGM(Admin), nullptr, 5, 0 },
// 	{ SPGM(Utilities), nullptr, 6, 0 },

// 	{ SPGM(Home), SPGM(index_html), 100, 1, },
// 	{ SPGM(Status), SPGM(status_html), 101, 1, },


// 	{ nullptr, nullptr, 0, 0  }
// };

BootstrapMenu::BootstrapMenu() : _unqiueId(0) {
}

BootstrapMenu::~BootstrapMenu() {
}

BootstrapMenu::menu_item_parent_id_t BootstrapMenu::addMenu(StaticString &&label, menu_item_id_t afterId)
{
	return _add(std::move(Item(*this, std::move(label), std::move(StaticString()))), afterId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::addSubMenu(StaticString &&label, StaticString &&uri, menu_item_parent_id_t parentMenuId, menu_item_id_t afterId)
{
	return _add(std::move(Item(*this, std::move(label), std::move(uri), parentMenuId)), afterId);
}

BootstrapMenu::ItemsVectorIterator BootstrapMenu::findMenuByLabel(const String &label, menu_item_parent_id_t menuId)
{
	return std::find(_items.begin(), _items.end(), FindHelper(label, menuId, FindHelper::LabelType()));
}

BootstrapMenu::ItemsVectorIterator BootstrapMenu::findMenuByURI(const String &uri, menu_item_parent_id_t menuId)
{
	return std::find(_items.begin(), _items.end(), FindHelper(uri, menuId, FindHelper::UriType()));
}

//BootstrapMenu::menu_item_id_t BootstrapMenu::getItemCount(menu_item_id_t menuId) const
//{
//	uint8_t count = 0;
//	for (auto &item : _items) {
//		if (item.getParentMenuId() == menuId) {
//			count++;
//		}
//	}
//	__LDBG_printf("BootstrapMenu::getItemCount(menuId=%d): count=%d", menuId, count);
//	return count;
//}

void BootstrapMenu::html(PrintInterface &output, ItemsVectorIterator top)
{
	if (isValid(top)) {
		auto menuId = top->getId();
		__LDBG_printf("menu_id=%d", menuId);
		if (top->hashURI()) {
			// menu bar only
			output.printf_P(PSTR("<li class=\"nav-item\">" "<a class=\"nav-link\" href=\"/%s\">%s</a></li>" BOOTSTRAP_MENU_CRLF), top->getUri().c_str(), top->getLabel().c_str());
		}
		else if (isValid(std::find(_items.begin(), _items.end(), FindHelper(top->getId(), FindHelper::ParentMenuIdType())))) {
			// drop down menu
			output.printf_P(PSTR(
				"<li class=\"nav-item dropdown\">" BOOTSTRAP_MENU_CRLF "<a class=\"nav-link dropdown-toggle\" href=\"#\" "
				"id=\"navbarDropdown%u\" role=\"button\" data-toggle=\"dropdown\" aria-haspopup=\"true\" aria-expanded=\"false\">%s</a>" BOOTSTRAP_MENU_CRLF
				"<div class=\"dropdown-menu\" aria-labelledby=\"navbarDropdownConfig\">" BOOTSTRAP_MENU_CRLF
			), menuId, top->getLabel().c_str());

			for (auto dropdown = top + 1; dropdown != _items.end(); ++dropdown) {
				if (dropdown->getParentMenuId() == menuId) {
					output.printf_P(PSTR("<a class=\"dropdown-item\" href=\"/%s\">%s</a>" BOOTSTRAP_MENU_CRLF), dropdown->getUri().c_str(), dropdown->getLabel().c_str());
				}
			}
			output.printf_P(PSTR("</div></li>" BOOTSTRAP_MENU_CRLF));
		}
		else {
			// empty drop down, do not display
		}
	}
}

void BootstrapMenu::html(PrintInterface &output)
{
	__LDBG_println();
	for (auto iterator = _items.begin(); iterator != _items.end(); ++iterator) {
		if (iterator->getParentMenuId() == InvalidMenuId) {
			html(output, iterator);
		}
	}
}

void BootstrapMenu::htmlSubMenu(PrintInterface &output, ItemsVectorIterator top, uint8_t active)
{
	if (isValid(top)) {
		auto menuId = top->getId();
		menu_item_id_t pos = 0;
		for (auto iterator = top + 1; iterator != _items.end(); ++iterator) {
			if (iterator->getParentMenuId() == menuId) {
				output.printf_P(PSTR(
						"<a href=\"/%s\" class=\"list-group-item list-group-item-action align-items-start%s\">"
						"<h5 class=\"mb-1\">%s</h5></a>" BOOTSTRAP_MENU_CRLF
					), iterator->getUri().c_str(), active == pos ? PSTR(" active") : emptyString.c_str(), iterator->getLabel().c_str()
				);
				pos++;
			}
		}
		__LDBG_printf("menu_id=%d active=%d items=%d", menuId, active, pos);
	}
}

BootstrapMenu::menu_item_id_t BootstrapMenu::_add(Item &item, menu_item_id_t afterId)
{
	return _add(std::move(item), afterId);
}

BootstrapMenu::menu_item_id_t BootstrapMenu::_add(Item &&item, menu_item_id_t afterId)
{
	auto prev = std::find(_items.begin(), _items.end(), FindHelper(afterId, FindHelper::MenuIdType()));
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
