/**
* Author: sascha_lammers@gmx.de
*/

//#if !_MSC_VER
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
//#endif

#include <PrintHtmlEntities.h>
#include <PrintHtmlEntitiesString.h>
#include <misc.h>
#include "WebUI/BaseUI.h"
#include "Field/BaseField.h"
#include "Form/BaseForm.h"
#include "WebUI/Config.h"

using namespace FormUI;

const char *WebUI::Config::encodeHtmlEntities(const char *cStr, bool attribute)
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

void WebUI::BaseUI::_addItem(const Container::CheckboxButtonSuffix &suffix)
{
    auto size = suffix._items.size();
    Storage::CStrVector _vector(size);
    auto ptr = _vector.data();
    for (auto &str : suffix._items) {
        if (--size == 0) {
            *ptr++ = encodeHtmlEntities(str);
        }
        else{
            *ptr++ = attachString(str);
        }
    }
    _storage.push_back(Storage::Type::SUFFIX_HTML, _vector.begin(), _vector.end());
}

bool WebUI::BaseUI::_compareValue(const char *value) const
{
    if (_parent->getType() == Field::Type::TEXT) {
        return String_equals(_parent->getValue().c_str(), value);
    }
    else {
        return String(FPSTR(value)).toInt() == _parent->getValue().toInt();
    }
}

void WebUI::BaseUI::_setItems(const Container::List &items)
{
    _storage.reserve(_storage.size() + (items.size() * (sizeof(Storage::Value::Option) + sizeof(Storage::Type))));
    for(const auto &item : items) {
        const char *value = _attachMixedContainer(item.second, AttachStringAsType::HTML_ENTITIES);
        if (item.first.isInt()) {
            _storage.push_back(Storage::Value::OptionNumKey(item.first.getInt(), value));
        }
        else {
            _storage.push_back(Storage::Value::Option(_attachMixedContainer(item.first, AttachStringAsType::HTML_ATTRIBUTE), value));
        }
    }
}

bool WebUI::BaseUI::_hasLabel() const
{
    return _storage.find_if(_storage.begin(), _storage.end(), Storage::Vector::isLabel) != _storage.end();
}

bool WebUI::BaseUI::_hasSuffix() const
{
    return _storage.find_if(_storage.begin(), _storage.end(), Storage::Vector::isSuffix) != _storage.end();
}

bool WebUI::BaseUI::_hasAttributes() const
{
    return _storage.find_if(_storage.begin(), _storage.end(), Storage::Vector::isAttribute) != _storage.end();
}

void WebUI::BaseUI::_addItem(const Container::StringAttribute &attribute)
{
    const char *key;
    const char *value;
    _storage.push_back(Storage::Value::Attribute(key = attachString(attribute._key), value = encodeHtmlEntities(attribute._value)));
    _checkDisableAttribute(key, value);
}

void WebUI::BaseUI::_addItem(const Container::FPStringAttribute &attribute)
{
    const char *key;
    const char *value;
    _storage.push_back(Storage::Value::Attribute(key = attachString(attribute._key), value = encodeHtmlEntities(attribute._value)));
    _checkDisableAttribute(key, value);
}


//#if !_MSC_VER
//#pragma GCC diagnostic pop
//#endif

#if KFC_FORMS_INCLUDE_HPP_AS_INLINE == 0

#include "WebUI/BaseUI.hpp"

#endif
