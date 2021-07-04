/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Form/BaseForm.h"
#include "Validator/BaseValidator.h"
#include "Utility/ForwardList.h"
#include "WebUI/Config.h"
#include "WebUI/BaseUI.h"
#include "WebUI/Storage.h"
#include "Utility/Debug.h"

using namespace FormUI;


__KFC_FORMS_INLINE_METHOD__
const char *WebUI::BaseUI::attachString(const char *str)
{
    return _parent->getWebUIConfig().attachString(str);
}


__KFC_FORMS_INLINE_METHOD__
const char *WebUI::BaseUI::encodeHtmlEntities(const char *str)
{
    return _parent->getWebUIConfig().encodeHtmlEntities(str);
}


__KFC_FORMS_INLINE_METHOD__
const char *WebUI::BaseUI::encodeHtmlAttribute(const char *str)
{
    return _parent->getWebUIConfig().encodeHtmlAttribute(str);
}


__KFC_FORMS_INLINE_METHOD__
StringDeduplicator &WebUI::BaseUI::strings()
{
    return _parent->getWebUIConfig().strings();
}


__KFC_FORMS_INLINE_METHOD__
bool WebUI::BaseUI::_isSelected(int32_t value) const
{
    auto val = _parent->getValue();
    if (val.length() == 0) {
        return false;
    }
    else if (value == 0) {
        return val.length() == 1 && val.charAt(0) == '0';
    }
    return value == val.toInt();
}


__KFC_FORMS_INLINE_METHOD__
bool WebUI::BaseUI::_compareTextValue(const char *value) const
{
    return _parent->getValue() == value;
}


__KFC_FORMS_INLINE_METHOD__
bool WebUI::BaseUI::_compareValue(const char *value) const
{
    if (_parent->getType() == Field::Type::TEXT) {
        return _compareTextValue(value);
    }
    else {
        char buf[16];
        strncpy_P(buf, value, sizeof(buf) - 1)[sizeof(buf) - 1] = 0;
        return _isSelected(atol(buf));
    }
}


__KFC_FORMS_INLINE_METHOD__
void FormUI::WebUI::BaseUI::_addItem(const Container::DisabledAttribute &attribute)
{
    attribute.push_back(_storage, *this);
    _parent->setDisabled(true);
}


__KFC_FORMS_INLINE_METHOD__
WebUI::BaseUI &Field::BaseField::setFormUI(WebUI::BaseUI *formUI)
{
    __LDBG_assert_printf(formUI != nullptr, "invalid baseui=%p", formUI);
    if (_formUI) {
        delete _formUI;
    }
    return *(_formUI = formUI);
}


__KFC_FORMS_INLINE_METHOD__
WebUI::BaseUI *Field::BaseField::getFormUI() const
{
    return _formUI;
}


__KFC_FORMS_INLINE_METHOD__
RenderType Field::BaseField::getRenderType() const
{
    if (_formUI) {
        return _formUI->getType();
    }
    return RenderType::NONE;
}


__KFC_FORMS_INLINE_METHOD__
void Field::BaseField::html(PrintInterface &output)
{
    if (_formUI) {
        _formUI->html(output);
    }
}


__KFC_FORMS_INLINE_METHOD__
void Field::BaseField::setDisabled(bool state)
{
    _disabled = state;
}


__KFC_FORMS_INLINE_METHOD__
bool Field::BaseField::isDisabled() const
{
    return _disabled;
}

__KFC_FORMS_INLINE_METHOD__
void Field::BaseField::setOptional(bool opts)
{
    _optional = opts;
}

__KFC_FORMS_INLINE_METHOD__
bool Field::BaseField::isOptional() const
{
    return _optional;
}


#include <debug_helper_disable.h>
