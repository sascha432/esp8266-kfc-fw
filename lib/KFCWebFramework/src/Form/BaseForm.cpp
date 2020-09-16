/**
* Author: sascha_lammers@gmx.de
*/

#include "Types.h"
#include "Form/BaseForm.h"
#include "Form/Data.h"
#include "Form/Error.h"
#include "Field/Group.h"
#include "Validator/BaseValidator.h"
#include "WebUI/Containers.h"
#include <JsonTools.h>
#include "Form/Form.hpp"

#include "Utility/Debug.h"

using namespace FormUI;

Field::BaseField *Form::BaseForm::getField(const String &name) const
{
    auto iterator = std::find_if(_fields.begin(), _fields.end(), [&name](const Field::Ptr &ptr) {
        return *ptr == name;
    });
    if (iterator != _fields.end()) {
        return iterator->get();
    }
    return nullptr;
}

// --------------------------------------------------------------------
// Form data handling
//

bool Form::BaseForm::validate()
{
    validateOnly();
    if (_hasChanged) {
        copyValidatedData();
    }
#if KFC_FORMS_HAVE_VALIDATE_CALLBACK
    if (_validateCallback) {
        return _validateCallback(*this);
    }
#endif
    return isValid();
}

bool Form::BaseForm::validateOnly()
{
    _hasChanged = false;
    clearErrors();
    for (const auto &_field: _fields) {
        auto field = _field.get();
        // __DBG_printf("name=%s disabled=%u", field->getName().c_str(), field->isDisabled());
        if (field->getType() != Field::Type::GROUP && field->isDisabled() == false) {
            if (_data->hasArg(field->getName())) {
                // __LDBG_printf("Form::BaseForm::validateOnly() Set value %s = %s", field->getName().c_str(), _data->arg(field->getName()).c_str());
                if (field->setValue(_data->arg(field->getName()))) {
                    _hasChanged = true;
                }
                auto iterator = _validatorFind(_validators, field);
                while (iterator) {
                    if (!iterator->validate()) {
                        _addError(field, iterator->getMessage());
                    }
                    iterator = _validatorFindNext(iterator, field);
                }
                //if (field->hasValidators()) {
                //    for (const auto &validator: field->getValidators()) {
                //        if (!validator->validate()) {
                //            _addError(field.get(), validator->getMessage());
                //        }
                //    }
                //}
            }
            else if (_invalidMissing) {
                _addError(field, FSPGM(Form_value_missing_default_message));
            }
        }
    }
#if DEBUG_KFC_FORMS
    dump(DEBUG_OUTPUT, "Form Dump: ");
#endif
    return isValid();
}

bool Form::BaseForm::hasError(Field::BaseField &field) const
{
    if (_errors) {
        for (const auto &error : *_errors) {
            if (error.is(field)) {
                return true;
            }
        }
    }
    return false;
}

WebUI::BaseUI &Form::BaseForm::addFormUI(WebUI::BaseUI *formUI)
{
    const auto &field = _fields.back();
    field->setFormUI(formUI);
    return *formUI;
}


Group &Form::BaseForm::addGroup(const String &name, const Container::Label &label, bool expanded, WebUI::Type type)
{
    auto &group = _add<Group>(name, expanded);
    group.setFormUI(&group, type, label);
    return group;
}

Group &Form::BaseForm::addGroup(const String &name, bool expanded, WebUI::Type type)
{
    auto &group = _add<Group>(name, expanded);
    group.setFormUI(&group, type);
    return group;
}

void Form::BaseForm::setFormUI(const String &title, const String &submit)
{
    auto &cfg = createWebUI();
    cfg.setTitle(title);
    cfg.setSaveButtonLabel(submit);
}

void Form::BaseForm::copyValidatedData()
{
    if (isValid()) {
        for (const auto &field : _fields) {
            if (field->isDisabled() == false) {
                field->copyValue();
            }
        }
    }
}

bool Form::BaseForm::process(const String &name, Print &output)
{
    // for (const auto &field : _fields) {
    //     Serial.printf_P(PSTR("name[%u]=%s "), field->getType(), field->getName().c_str());
    // }
    // Serial.println();
    for (const auto &field : _fields) {
        auto len = field->getName().length();
        if (field->getType() == Field::Type::TEXT && name.equalsIgnoreCase(field->getName())) {
            __LDBG_printf("name=%s text=%s", name.c_str(), field->getValue().c_str());
            PrintHtmlEntities::printTo(PrintHtmlEntities::Mode::ATTRIBUTE, field->getValue().c_str(), output);
            return true;
        }
        else if (field->getType() == Field::Type::TEXTAREA && name.equalsIgnoreCase(field->getName())) {
            __LDBG_printf("name=%s textarea=%s", name.c_str(), field->getValue().c_str());
            PrintHtmlEntities::printTo(PrintHtmlEntities::Mode::HTML, field->getValue().c_str(), output);
            return true;
        }
        else if (field->getType() == Field::Type::CHECK && name.equalsIgnoreCase(field->getName())) {
            __LDBG_printf("name=%s checkbox=%d", name.c_str(), field->getValue().toInt());
            if (field->getValue().toInt()) {
                output.print(FSPGM(_checked, " checked"));
            }
            return true;
        }
        else if (field->getType() == Field::Type::SELECT && strncasecmp(field->getName().c_str(), name.c_str(), len) == 0) {
            if (name.length() == len) {
                __LDBG_printf("name=%s select=%s", name.c_str(), field->getValue().c_str());
                PrintHtmlEntities::printTo(PrintHtmlEntities::Mode::ATTRIBUTE, field->getValue().c_str(), output);
                return true;
            }
            else if (name.charAt(len) == '_') {
                auto value = name.substring(len + 1).toInt();
                __LDBG_printf("name=%s select=%d ? %d", name.c_str(), value, (int)field->getValue().toInt());
                if (value == field->getValue().toInt()) {
                    output.print(FSPGM(_selected));
                }
                return true;
            }
        }
    }
    __LDBG_printf("name=%s not found", name.c_str());
    return false;
}

const char *Form::BaseForm::jsonEncodeString(const String &str, PrintInterface &output)
{
    size_t jsonLen = JsonTools::lengthEscaped(str);
    if (jsonLen == str.length()) {
        return _strings.attachString(str.c_str());
    }
    else {
        PrintString encoded;
        encoded.reserve(jsonLen);
        // encodes the name and attached it to the PrintInterface
        JsonTools::printToEscaped(encoded, str);
        return _strings.attachString(encoded);
    }
}

void Form::BaseForm::createJavascript(PrintInterface &output)
{
#if DEBUG_KFC_FORMS
    MicrosTimer dur2;
    dur2.start();
#endif
    if (!isValid()) {
        __LDBG_printf("errors=%d", _errors->size());
        output.printf_P(PSTR("<script>" "$.formValidator.addErrors("));
        uint16_t idx = 0;
        for (auto &error: *_errors) {
            output.printf_P(PSTR("%c{'target':'#%s','error':'%s'}"), idx++ ? ',' : '[', jsonEncodeString(error.getName(), output), jsonEncodeString(error.getMessage(), output));
        }
        output.printf_P(PSTR("]);" "</script>"));
    }
#if DEBUG_KFC_FORMS
    __DBG_printf("render=form_javascript time=%.3fms", dur2.getTime() / 1000.0);
#endif
}

WebUI::Config &Form::BaseForm::createWebUI()
{
    if (_uiConfig == nullptr) {
        _uiConfig = new WebUI::Config(_strings);
    }
    return *_uiConfig;
}

void Form::BaseForm::dump(Print &out, const String &prefix) const {

    out.print(prefix);
    out.println(F("Errors:"));
    if (_errors == nullptr) {
        out.print(prefix);
        out.println(FSPGM(None, "None"));
    }
    else {
        for (auto &error : *_errors) {
            out.print(prefix);
            out.print(error.getName());
            out.print(F(": "));
            out.println(error.getMessage());
        }
    }
    out.print(prefix);
    if (_hasChanged) {
        out.println(F("Form data was modified"));
    }
    else {
        out.println(F("Form data was not modified"));
    }
    out.print(prefix);
    out.println(F("Form data:"));
    for (const auto &field : _fields) {
        out.print(prefix);
        out.print(field->getName());
        if (field->hasChanged()) {
            out.print('*');
        }
        if (hasError(*field)) {
            out.print('#');
        }
        out.print(F(": '"));
        out.print(field->getValue());
        out.println('\'');
    }
}

#if KFC_FORMS_INCLUDE_HPP_AS_INLINE == 0

#include "Form/BaseForm.hpp"

#endif
