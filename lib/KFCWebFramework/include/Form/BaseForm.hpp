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
void Form::BaseForm::clearErrors()
{
    _clearErrors();
}


__KFC_FORMS_INLINE_METHOD__
bool Form::BaseForm::isValid() const
{
    return _errors == nullptr || _errors->empty();
}


__KFC_FORMS_INLINE_METHOD__
bool Form::BaseForm::hasErrors() const
{
    return !isValid();
    // return _errors != nullptr && _errors->empty() == false;
}


__KFC_FORMS_INLINE_METHOD__
bool Form::BaseForm::hasChanged() const
{
    return _hasChanged;
}


__KFC_FORMS_INLINE_METHOD__
const Form::Error::Vector &Form::BaseForm::getErrors() const
{
    if (_errors == nullptr) {
        __LDBG_assert_printf(_errors != nullptr, "_errors=nullptr getErrors() called with hasErrors() == false / isValid() == true");
        const_cast<BaseForm *>(this)->_errors = new ErrorVector();
    }
    return *_errors;
}

__KFC_FORMS_INLINE_METHOD__
void Form::BaseForm::finalize() const
{
#if DEBUG_KFC_FORMS
    __LDBG_printf("render=create_form time=%.3fms", _duration.getTimeConst() / 1000.0);
#endif
    // if (hasWebUIConfig()) {
    //     for (const auto &field : _fields) {
    //         int n = 0;
    //         Serial.print(field->getName());
    //         Serial.print('=');
    //         for (uint8_t byte : field->_formUI->_storage) {
    //             Serial.printf("%02x", byte);
    //             if (++n == 38) {
    //                 Serial.println();
    //             }
    //         }
    //         Serial.println();
    //     }
    // }
#if DEBUG_KFC_FORMS
    // dump(DEBUG_OUTPUT, "Form Dump: ");
#endif
}


__KFC_FORMS_INLINE_METHOD__
void Form::BaseForm::setFormUI(const String &title)
{
    createWebUI().setTitle(title);
}


__KFC_FORMS_INLINE_METHOD__
Validator::BaseValidator *Form::BaseForm::_validatorFind(Validator::BaseValidator *iterator, Field::BaseField *field)
{
    if (iterator) {
        return iterator->find(iterator, [field](const Validator::BaseValidator &validator) {
            return validator == field;
        });
    }
    return nullptr;
}


__KFC_FORMS_INLINE_METHOD__
Validator::BaseValidator *Form::BaseForm::_validatorFindNext(Validator::BaseValidator *iterator, Field::BaseField *field)
{
    Validator::BaseValidator::next(iterator, [field](const Validator::BaseValidator &validator) {
        return validator == field;
    });
    return nullptr;
}


__KFC_FORMS_INLINE_METHOD__
void Form::BaseForm::_addValidator(Validator::BaseValidator *validator)
{
    Validator::BaseValidator::push_back(&_validators, validator);
}


__KFC_FORMS_INLINE_METHOD__
void Form::BaseForm::_clearValidators()
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

#include <debug_helper_disable.h>
