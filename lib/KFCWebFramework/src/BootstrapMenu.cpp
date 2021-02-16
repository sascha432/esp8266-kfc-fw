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

void BootstrapMenu::html(PrintInterface &output, ItemsVectorIterator top, uint8_t level)
{
	if (isValid(top)) {
		auto menuId = top->getId();
		if (top->hasURI() && level == 0) {
		    __LDBG_printf("menubar menu_id=%d", menuId);
			// menu bar only
			output.printf_P(PSTR("<li class=\"nav-item\">" "<a class=\"nav-link\" href=\"/%s\">%s</a></li>" BOOTSTRAP_MENU_CRLF), top->getUri().c_str(), top->getLabel().c_str());
		}
		else if (isValid(std::find(_items.begin(), _items.end(), FindHelper(top->getId(), FindHelper::ParentMenuIdType())))) {
		    __LDBG_printf("menu level=%u menu_id=%d", level, menuId);
            if (level == 0) {
                // drop down menu
                output.printf_P(PSTR("<li class=\"nav-item dropdown\">" BOOTSTRAP_MENU_CRLF "<a class=\"nav-link dropdown-toggle\" href=\"#\" id=\"navbarDropdown"
                    "%u\" role=\"button\" data-toggle=\"dropdown\" aria-haspopup=\"true\" aria-expanded=\"false\">%s</a>" BOOTSTRAP_MENU_CRLF
                    "<ul class=\"dropdown-menu\">" BOOTSTRAP_MENU_CRLF
                ), menuId, top->getLabel().c_str());
            }
            else {
                // sub menu
                output.printf_P(PSTR("<li class=\"dropdown-submenu\">" BOOTSTRAP_MENU_CRLF
                    "<a class=\"dropdown-item dropdown-toggle\" href=\"/#\">%s</a>" BOOTSTRAP_MENU_CRLF
                    "<ul class=\"dropdown-menu\">" BOOTSTRAP_MENU_CRLF
                    ), top->getLabel().c_str());

            }
			for (auto dropdown = top + 1; dropdown != _items.end(); ++dropdown) {
                if (dropdown->getParentMenuId() == menuId) {
                    if (!dropdown->hasLabel()) {
                        output.printf_P(PSTR("<li><div class=\"dropdown-divider\"></div>"));
                    }
                    else if (dropdown->hasURI()) {
                        output.printf_P(PSTR("<li><a class=\"dropdown-item\" href=\"/%s\">%s</a></li>" BOOTSTRAP_MENU_CRLF), dropdown->getUri().c_str(), dropdown->getLabel().c_str());
                    }
                    else {
                        html(output, dropdown, level + 1);
                    }
                }
			}
			output.print(F("</ul></li>" BOOTSTRAP_MENU_CRLF));
		}
		else {
			// empty drop down, do not display
            __LDBG_printf("empty=%d", menuId);
		}
	}
}

void BootstrapMenu::html(PrintInterface &output)
{
#if DEBUG_BOOTSTRAP_MENU_RENDER_TIME
    MicrosTimer dur;
    dur.start();
#endif
	__LDBG_println();
	for (auto iterator = _items.begin(); iterator != _items.end(); ++iterator) {
		if (iterator->getParentMenuId() == InvalidMenuId) {
			html(output, iterator);
		}
	}
#if DEBUG_BOOTSTRAP_MENU_RENDER_TIME
    __DBG_printf("render=bootstrap_menu time=%.3fms", dur.getTime() / 1000.0);
#endif
}

void BootstrapMenu::htmlSubMenu(PrintInterface &output, ItemsVectorIterator top, uint8_t active)
{
    if (!isValid(top)) {
        __LDBG_print("top iterator invalid");
        return;
    }
#if DEBUG_BOOTSTRAP_MENU_RENDER_TIME
    MicrosTimer dur;
    dur.start();
#endif
    auto menuId = top->getId();
    menu_item_id_t pos = 0;
    for (auto iterator = std::next(top); iterator != _items.end(); ++iterator) {
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
#if DEBUG_BOOTSTRAP_MENU_RENDER_TIME
    __DBG_printf("render=bootstrap_menu sub=%d time=%.3fms", menuId, dur.getTime() / 1000.0);
#endif
}

// BootstrapMenu::menu_item_id_t BootstrapMenu::_add(const Item &item, menu_item_id_t afterId)
// {
// 	return _add(item, afterId);
// }

BootstrapMenu::menu_item_id_t BootstrapMenu::_add(Item &&item, menu_item_id_t afterId)
{
	auto insertAfter = std::find(_items.begin(), _items.end(), FindHelper(afterId, FindHelper::MenuIdType()));
	if (insertAfter != _items.end()) {
        insertAfter = std::next(insertAfter);
    }
	return _items.emplace(insertAfter, std::move(item))->getId();
}

BootstrapMenu::menu_item_id_t BootstrapMenu::_getUnqiueId()
{
	return ++_unqiueId;
}
