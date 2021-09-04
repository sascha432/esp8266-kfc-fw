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

#if 0
#    include <debug_helper_disable_ptr_validation.h>
#endif

using namespace FormUI;

const char *WebUI::Config::encodeHtmlEntities(const char *cStr, Mode mode)
{
    __DBG_validatePointerCheck(cStr, VP_HPS);
    uint8_t byte = pgm_read_byte(cStr);
    if (byte == 0xff) { // marker for html
        __LDBG_assert_printf(F("marker deprecated") == nullptr, "marker deprecated");
        return strings().attachString(cStr) + 1;
    }
    // if (byte == '<') { // marker for html
    //     return strings().attachString(cStr);
    // }

    int size;
    if ((size = PrintHtmlEntities::getTranslatedSize_P(cStr, mode)) != PrintHtmlEntities::kNoTranslationRequired) {
        String target;
        if (PrintHtmlEntities::translateTo(cStr, target, mode, size)) {
            return strings().attachString(target);
        }
    }
    return strings().attachString(cStr);
}

void WebUI::BaseUI::_addItem(const Container::SelectSuffix &suffix)
{
    __LDBG_assert_printf(suffix._items.empty() == false, "empty list");
    if (suffix._items.empty()) {
        return;
    }
    Storage::TypeByte tb(Storage::Value::SuffixHtml::type, suffix._items.size());
    _storage.reserve_extend(tb.size());
    __LDBG_printf("items=%u size=%u vector=%u", suffix._items.size(), tb.size(), _storage.size());

    auto target = std::back_inserter<Storage::VectorBase>(_storage);
    *target = tb.toByte();
    ++target;

    for(const auto &item: suffix._items) {
        Storage::Value::SuffixHtml(attachString(item.getValue())).push_back(target);
    }
    _storage.push_back(Storage::Value::SuffixHtml(attachString(F("</select>"))));
}

void WebUI::BaseUI::_addItem(const Container::TextInputSuffix &suffix)
{
    __LDBG_assert_printf(suffix._items.empty() == false, "empty list");
    if (suffix._items.empty()) {
        return;
    }
    Storage::TypeByte tb(Storage::Value::SuffixHtml::type, suffix._items.size());
    _storage.reserve_extend(tb.size());
    __LDBG_printf("items=%u size=%u vector=%u", suffix._items.size(), tb.size(), _storage.size());

    auto target = std::back_inserter<Storage::VectorBase>(_storage);
    *target = tb.toByte();
    ++target;

    for(const auto &item: suffix._items) {
        Storage::Value::SuffixHtml(attachString(item.getValue())).push_back(target);
    }
}

void WebUI::BaseUI::_addItem(const Container::CheckboxButtonSuffix &suffix)
{
    __LDBG_assert_printf(suffix._items.empty() == false, "empty list");
    if (suffix._items.empty()) {
        return;
    }
    Storage::TypeByte tb(Storage::Value::SuffixHtml::type, suffix._items.size());
    _storage.reserve_extend(tb.size());
    __LDBG_printf("items=%u size=%u vector=%u", suffix._items.size(), tb.size(), _storage.size());

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
}

void WebUI::BaseUI::_setItems(const Container::List &items)
{
    _storage.reserve_extend(items.size() * Storage::TypeByte::size_for<Storage::Value::Option>());
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
#include "WebUI/Container.hpp"

#endif
