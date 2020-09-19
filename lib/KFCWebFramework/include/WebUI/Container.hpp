/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Form/BaseForm.h"
#include "Validator/BaseValidator.h"
#include "Utility/ForwardList.h"
#include "WebUI/Config.h"
#include "WebUI/BaseUI.h"

using namespace FormUI;

#include "Utility/Debug.h"


__KFC_FORMS_INLINE_METHOD__
void AttributeTmpl<int32_t>::push_back(Storage::Vector &storage, WebUI::BaseUI &ui) const
{
    storage.push_back(Storage::Value::AttributeInt(ui.attachString(_key), _value));
}


__KFC_FORMS_INLINE_METHOD__
void AttributeTmpl<const __FlashStringHelper *>::push_back(Storage::Vector &storage, WebUI::BaseUI &ui) const 
{
    storage.push_back(Storage::Value::Attribute(ui.attachString(_key), ui.encodeHtmlEntities(_value)));
}


__KFC_FORMS_INLINE_METHOD__
void AttributeTmpl<String>::push_back(Storage::Vector &storage, WebUI::BaseUI &ui) const
{
    storage.push_back(Storage::Value::Attribute(ui.attachString(_key), ui.encodeHtmlEntities(_value)));
}

#include <debug_helper_disable.h>
