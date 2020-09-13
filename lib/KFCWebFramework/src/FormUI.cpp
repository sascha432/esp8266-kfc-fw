/**
* Author: sascha_lammers@gmx.de
*/

#if !_MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "FormUI.h"
#include "FormField.h"
#include "Form.h"
#include <PrintHtmlEntities.h>
#include <PrintHtmlEntitiesString.h>
#include <misc.h>

namespace FormUI {

    const char *Config::encodeHtmlEntities(const char *cStr, bool attribute)
    {
        uint8_t byte = pgm_read_byte(cStr);
        if (byte == 0xff || byte == '<') { // marker for html
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

    void UI::_addItem(const CheckboxButtonSuffix &suffix)
    {
        auto size = suffix._items.size();
        ItemsStorage::CStrVector _vector(size);
        auto ptr = _vector.data();
        for (auto &str : suffix._items) {
            if (--size == 0) {
                *ptr++ = encodeHtmlEntities(str);
            }
            else{
                *ptr++ = attachString(str);
            }
        }
        _storage.push_back(ItemsStorage::StorageType::SUFFIX_HTML, _vector.begin(), _vector.end());
    }

    bool UI::_isSelected(int32_t value) const
    {
        return value == _parent->getValue().toInt();
    }

    bool UI::_compareValue(const char *value) const
    {
        if (_parent->getType() == FormField::Type::TEXT) {
            return String_equals(_parent->getValue().c_str(), value);
        }
        else {
            return String(FPSTR(value)).toInt() == _parent->getValue().toInt();
        }
    }

    void UI::_setItems(const ItemsList &items)
    {
        //_storage.erase(std::remove(_storage.begin(), _storage.end(), PointerStorage::Type::OPTION), _storage.end());
        _storage.reserve(_storage.size() + items.size() * (sizeof(ItemsStorage::Option) + 1));
		for(const auto &item : items) {
            const char *value = _attachMixedContainer(item.second, AttachStringAsType::HTML_ENTITIES);
            if (item.first.isInt()) {
                _storage.push_back(ItemsStorage::OptionNumKey(item.first.getInt(), value));
            }
            else {
                _storage.push_back(ItemsStorage::Option(_attachMixedContainer(item.first, AttachStringAsType::HTML_ATTRIBUTE), value));
            }
        }
    }

    bool UI::_hasLabel() const
    {
        return _storage.find(_storage.begin(), _storage.end(), ItemsStorage::isLabel) != _storage.end();
    }

    bool UI::_hasSuffix() const
    {
        return _storage.find(_storage.begin(), _storage.end(), ItemsStorage::isSuffix) != _storage.end();
    }

    bool UI::_hasAttributes() const
    {
        return _storage.find(_storage.begin(), _storage.end(), ItemsStorage::isAttribute) != _storage.end();
    }
                
}

#if !_MSC_VER
#pragma GCC diagnostic pop
#endif

