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
const char *FormUI::WebUI::BaseUI::attachString(const char *str)
{
    return _parent->getWebUIConfig().attachString(str);
}


__KFC_FORMS_INLINE_METHOD__
const char *FormUI::WebUI::BaseUI::encodeHtmlEntities(const char *str)
{
    return _parent->getWebUIConfig().encodeHtmlEntities(str);
}


__KFC_FORMS_INLINE_METHOD__
const char *FormUI::WebUI::BaseUI::encodeHtmlAttribute(const char *str)
{
    return _parent->getWebUIConfig().encodeHtmlAttribute(str);
}


__KFC_FORMS_INLINE_METHOD__
StringDeduplicator &WebUI::BaseUI::strings()
{
    return _parent->getWebUIConfig().strings();
}


__KFC_FORMS_INLINE_METHOD__
bool FormUI::WebUI::BaseUI::_isSelected(int32_t value) const
{
    return value == _parent->getValue().toInt();
}


__KFC_FORMS_INLINE_METHOD__
void FormUI::WebUI::BaseUI::_checkDisableAttribute(const char *key, const char *value)
{
    if (_isDisabledAttribute(key, value)) {
        _parent->setDisabled(true);
    }
}

#include <debug_helper_disable.h>
