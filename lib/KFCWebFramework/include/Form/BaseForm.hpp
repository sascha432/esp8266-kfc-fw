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

__KFC_FORMS_INLINE_METHOD__
Group &Form::BaseForm::addGroup(const String &name, const Container::Label &label, bool expanded, WebUI::Type type)
{
    auto &group = _add<Group>(name, expanded);
    group.setFormUI(&group, type, label);
    return group;
}


__KFC_FORMS_INLINE_METHOD__
WebUI::BaseUI &Form::BaseForm::addFormUI(WebUI::BaseUI *formUI)
{
    const auto &field = _fields.back();
    field->setFormUI(formUI);
    return *formUI;
}


__KFC_FORMS_INLINE_METHOD__
WebUI::Config &FormUI::Form::BaseForm::createWebUI()
{
    if (_uiConfig == nullptr) {
        _uiConfig = new WebUI::Config(_strings);
    }
    return *_uiConfig;
}

__KFC_FORMS_INLINE_METHOD__
void Form::BaseForm::setFormUI(const String &title, const String &submit)
{
    auto &cfg = createWebUI();
    cfg.setTitle(title);
    cfg.setSaveButtonLabel(submit);
}


__KFC_FORMS_INLINE_METHOD__
const Form::Error::Vector &Form::BaseForm::getErrors() const
{
    if (_errors == nullptr) {
        __DBG_panic("errors vector is nullptr");
    }
    return *_errors;
}

__KFC_FORMS_INLINE_METHOD__
void Form::BaseForm::finalize() const
{
    if (hasWebUIConfig()) {
        for (const auto &field : _fields) {
            int n = 0;
            Serial.print(field->getName());
            Serial.print('=');
            for (uint8_t byte : field->_formUI->_storage) {
                Serial.printf("%02x", byte);
                if (++n == 38) {
                    Serial.println();
                }
            }
            Serial.println();
            
        }
    }
#if DEBUG_KFC_FORMS
    dump(DEBUG_OUTPUT, "Form Dump: ");
#endif
}


__KFC_FORMS_INLINE_METHOD__
void Form::BaseForm::setFormUI(const String &title)
{
    createWebUI().setTitle(title);
}


__KFC_FORMS_INLINE_METHOD__
Validator::BaseValidator *FormUI::Form::BaseForm::_validatorFind(Validator::BaseValidator *iterator, Field::BaseField *field)
{
    if (iterator) {
        return iterator->find(iterator, [field](const Validator::BaseValidator &validator) {
            return validator == field;
        });
    }
    return nullptr;
}


__KFC_FORMS_INLINE_METHOD__
Validator::BaseValidator *FormUI::Form::BaseForm::_validatorFindNext(Validator::BaseValidator *iterator, Field::BaseField *field)
{
    Validator::BaseValidator::next(iterator, [field](const Validator::BaseValidator &validator) {
        return validator == field;
    });
    return nullptr;
}


__KFC_FORMS_INLINE_METHOD__
void FormUI::Form::BaseForm::_addValidator(Validator::BaseValidator *validator)
{
    Validator::BaseValidator::push_back(&_validators, validator);
}


__KFC_FORMS_INLINE_METHOD__
void FormUI::Form::BaseForm::_clearValidators()
{
    Validator::BaseValidator::clear(&_validators);
}

__KFC_FORMS_INLINE_METHOD__
Form::Data *Form::BaseForm::getFormData() const
{
    return _data;
}


#if KFC_FORMS_HAVE_VALIDATE_CALLBACK

__KFC_FORMS_INLINE_METHOD__
void Form::BaseForm::setValidateCallback(Validator::Callback validateCallback)
{
    _validateCallback = validateCallback;
}

#endif
