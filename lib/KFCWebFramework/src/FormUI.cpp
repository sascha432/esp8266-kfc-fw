/**
* Author: sascha_lammers@gmx.de
*/

#if !_MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "FormUI.h"
#include "FormField.h"
#include <PrintHtmlEntities.h>
#include <misc.h>

namespace FormUI {

    const char *Config::encodeHtmlEntities(const char *cStr, bool attribute)
    {
        if (pgm_read_byte(cStr) == 0xff) {
            return strings().attachString(cStr) + 1;
        }

        int size;
        if ((size = PrintHtmlEntities::getTranslatedSize_P(cStr, attribute)) != PrintHtmlEntities::kNoTranslationRequired) {
            String target;
            if (PrintHtmlEntities::translateTo(cStr, target, attribute, size)) {
                return strings().attachString(target);
            }
        }
        return strings().attachString(cStr);
    }

    StringPairList::StringPairList(Config &ui, const ItemsList &items)
    {
        reserve(items.size());
		for(const auto &item : items) {
            emplace_back(ui.strings().attachString(item.first), ui.encodeHtmlEntities(item.second, false));
        }
    }


    UI *UI::setBoolItems()
    {
        return setBoolItems(FSPGM(Enabled), FSPGM(Disabled));
    }

    UI *UI::setBoolItems(const String &enabled, const String &disabled)
    {
        _setItems(ItemsList(0, disabled, 1, enabled));
        return this;
    }

    UI *UI::addItems(const ItemsList &items)
    {
        _setItems(items);
        return this;
    }

    UI *UI::setSuffix(const String &suffix)
    {
        _suffix = _parent->getFormUIConfig().strings().attachString(suffix);
        return this;
    }

    UI *UI::setPlaceholder(const String &placeholder)
    {
        addAttribute(FSPGM(placeholder), placeholder);
        return this;
    }

    UI *UI::setMinMax(const String &min, const String &max)
    {
        addAttribute(FSPGM(min), min);
        addAttribute(FSPGM(max), max);
        return this;
    }

    UI *UI::addAttribute(const __FlashStringHelper *name, const String &value)
    {
        _attributes += ' ';
        _attributes += name;
        _attributes.reserve(_attributes.length() + value.length() + 4);
        if (value.length()) {
            _attributes += '=';
            _attributes += '"';
            // append translated value to _attributes
            if (!PrintHtmlEntities::translateTo(value.c_str(), _attributes, true)) {
                _attributes += value; // no translation required, just append value
            }
            _attributes += '"';
        }
        return this;
    }

    UI *UI::addConditionalAttribute(bool cond, const __FlashStringHelper *name, const String &value)
    {
        if (cond) {
            return addAttribute(name, value);
        }
        return this;
    }

    bool UI::_compareValue(const String &value) const
    {
        if (_parent->getType() == FormField::Type::TEXT) {
            return value.equals(_parent->getValue());
        }
        else {
            return value.toInt() == _parent->getValue().toInt();
        }
    }

    void UI::_setItems(const ItemsList &items)
    {
        if (_items) {
            delete _items;
        }
        _items = new StringPairList(_parent->getFormUIConfig(), items);
    }

    const char *UI::_getAttributes()
    {
        auto str = _parent->getFormUIConfig().strings().attachString(_attributes);
        return str;
    }

    char UI::_hasLabel() const
    {
        if (!_label) {
            return 0;
        }
        return pgm_read_byte(_label);
    }

    char UI::_hasSuffix() const
    {
        if (!_suffix) {
            return 0;
        }
        return pgm_read_byte(_suffix);
    }

}

#if !_MSC_VER
#pragma GCC diagnostic pop
#endif

