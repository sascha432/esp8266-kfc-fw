/**
* Author: sascha_lammers@gmx.de
*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "FormUI.h"
#include "FormField.h"
#include <PrintHtmlEntities.h>
#include <misc.h>

namespace FormUI {

    UI *UI::setBoolItems()
    {
        return setBoolItems(FSPGM(Enabled), FSPGM(Disabled));
    }

    UI *UI::setBoolItems(const String &enabled, const String &disabled)
    {
        _items.clear();
        _items.reserve(2);
        _items.emplace_back(std::move(String(0)), disabled);
        _items.emplace_back(std::move(String(1)), enabled);
        return this;
    }

    UI *UI::addItems(const String &value, const String &label)
    {
        _items.push_back(value, label);
        return this;
    }

    UI *UI::addItems(const ItemsList &items)
    {
        _items = items;
        return this;
    }

    UI *UI::setSuffix(const String &suffix)
    {
        _suffix = suffix;
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

    UI *UI::addAttribute(const String &name, const String &value)
    {
        _attributes += ' ';
        _attributes += name;
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

    UI *UI::addAttribute(const __FlashStringHelper *name, const String &value)
    {
        _attributes += ' ';
        _attributes += name;
        if (value.length()) {
            _attributes += '=';
            _attributes += '"';
            if (!PrintHtmlEntities::translateTo(value.c_str(), _attributes, true)) {
                _attributes += value;
            }
            _attributes += '"';
        }
        return this;
    }

    UI *UI::addConditionalAttribute(bool cond, const String &name, const String &value)
    {
        if (cond) {
            return addAttribute(name, value);
        }
        return this;
    }

    UI *UI::setReadOnly()
    {
        return addAttribute(FSPGM(readonly), String());
    }

    const char *UI::_encodeHtmlEntities(const char *cStr, bool attribute, PrintInterface &output)
    {
        // __DBG_printf("ptr=%p %s attribute=%u", cStr, cStr, attribute);
        if (pgm_read_byte(cStr) == 0xff) {
            // __DBG_printf("raw=%s", cStr + 1);
            return ++cStr;
        }
        const char *attachedStr;
        if ((attachedStr = output.getAttachedString(cStr)) != nullptr) {
            // __DBG_printf("attached=%p %s", attachedStr, attachedStr);
            return attachedStr;
        }
        int size;
        if ((size = PrintHtmlEntities::getTranslatedSize_P(cStr, attribute)) == PrintHtmlEntities::kNoTranslationRequired) {
            // __DBG_printf("no translation=%p %s", cStr, cStr);
            return cStr;
        }
        char *target;
        if ((target = PrintHtmlEntities::translateTo(cStr, attribute, size)) != nullptr) {
            // __DBG_printf("from=%s to=%s", cStr, target);
            if ((attachedStr = output.getAttachedString(target)) != nullptr) { // check if we have this string already
                // __DBG_printf("attached_after=%p %s", attachedStr, attachedStr);
                free(target);
                return attachedStr;
            }
            auto result = output.attachString(target, true);
            // __DBG_printf("attachstring=%p %s", result, result);
            return result;
        }
        // __DBG_printf("nochange=%p %s", cStr, cStr);
        return cStr;
    }

    void UI::setParent(FormField *field)
    {
        _parent = field;
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

}

#pragma GCC diagnostic pop
