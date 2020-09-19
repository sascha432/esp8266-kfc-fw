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

using namespace FormUI;

#include "Utility/Debug.h"

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
    return String_equals(_parent->getValue(), value);
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
    _addItem(static_cast<const FPStringAttribute &>(attribute));
    _parent->setDisabled(true);
}

#include <debug_helper_disable.h>
