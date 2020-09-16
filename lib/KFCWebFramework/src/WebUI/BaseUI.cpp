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
#include "Form/Form.hpp"

#include "Utility/Debug.h"

using namespace FormUI;

const char *WebUI::Config::encodeHtmlEntities(const char *cStr, bool attribute)
{
    uint8_t byte = pgm_read_byte(cStr);
    if (byte == 0xff) { // marker for html
        return strings().attachString(cStr) + 1;
    }
    // if (byte == '<') { // marker for html
    //     return strings().attachString(cStr);
    // }

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
    if (suffix._items.size() == 0) {
        return;
    }
    Storage::TypeByte tb(Storage::Value::SuffixHtml::type, suffix._items.size());
    _storage.reserve_extend(tb.size());
    __LDBG_printf("items=%u size=%u vector=%u", suffix._items.size(), tb.size(), _storage.size());

#if KFC_FORMS_NO_DIRECT_COPY
    auto target = std::back_inserter<Storage::VectorBase>(_storage);
    *target = tb.toByte();
    ++target;

    auto &items = suffix._items;
    auto iterator = items.begin();
    while(true) {
        auto str = iterator->getValue();
        if (++iterator == items.end()) {
            Storage::Value::SuffixHtml(encodeHtmlEntities(str)).push_back(target);
            break;
        }
        else {
            Storage::Value::SuffixHtml(attachString(str)).push_back(target);
        }
    }
#else
    auto vSize = _storage.size();
    _storage.resize(vSize + tb.size());
    auto beginPtr = _storage.data() + vSize;
    *beginPtr = tb.toByte();
    auto target = (const char **)(&beginPtr[1]);
    static_assert(sizeof(Storage::Value::SuffixHtml) == sizeof(*target), "size does not match");

    uint8_t size = (uint8_t)suffix._items.size();
    for (const auto &mStr : suffix._items) {
        auto str = mStr.getValue();
        if (--size == 0) {
            str = encodeHtmlEntities(str);
        }
        else{
            str = attachString(str);
        }
        //std::copy_n(&ptr, 1, target++);
        memcpy(target++, &str, sizeof(const char *));
    }
#endif
    // __LDBG_printf("vector=%u", _storage.size());
}

void WebUI::BaseUI::_setItems(const Container::List &items)
{
    _storage.reserve_extend((items.size() * sizeof(Storage::Value::Option)) + sizeof(Storage::Type));
    static_assert(sizeof(Storage::Value::Option) == sizeof(Storage::Value::OptionNumKey), "size does not match");
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

//#if !_MSC_VER
//#pragma GCC diagnostic pop
//#endif

#if KFC_FORMS_INCLUDE_HPP_AS_INLINE == 0

#include "WebUI/BaseUI.hpp"

#endif
