/**
* Author: sascha_lammers@gmx.de
*/

#include "Form.h"
#include "FormData.h"
#include "FormError.h"
#include "FormValidator.h"
#include <JsonTools.h>

#if DEBUG_KFC_FORMS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Form::Form(FormData *data) : _data(data), _invalidMissing(true), _hasChanged(false), _uiConfig(nullptr)
{
}

void Form::clearForm()
{
    _fields.clear();
    _errors.clear();
}

void Form::setFormData(FormData *data)
{
    _data = data;
}

void Form::setInvalidMissing(bool invalidMissing)
{
    _invalidMissing = invalidMissing;
}

FormGroup &Form::addGroup(const String &name, const FormUI::Label &label, bool expanded, FormUI::Type type)
{
    auto &group = _add(new FormGroup(name, expanded));
    group.setFormUI(&group, type, label);
    return group;
}

FormField &Form::_add(FormField *field)
{
    field->setForm(this);
    _fields.emplace_back(field);
    return *_fields.back();
}

FormUI::UI &Form::addFormUI(FormUI::UI *formUI)
{
    const auto &field = _fields.back();
    field->setFormUI(formUI);
    return *formUI;
}

FormField *Form::getField(const String &name) const
{
    auto iterator = std::find_if(_fields.begin(), _fields.end(), [&name](const FormFieldPtr &ptr) {
        return *ptr == name;
    });
    if (iterator != _fields.end()) {
        return iterator->get();
    }
    return nullptr;
}

FormField &Form::getField(int index) const
{
    return *_fields.at(index);
}

size_t Form::hasFields() const
{
    return _fields.size();
}

void Form::removeValidators()
{
    for (const auto &field: _fields) {
        field->getValidators().clear();
    }
}

void Form::clearErrors()
{
    _errors.clear();
}

bool Form::validate()
{
    validateOnly();
    if (_hasChanged) {
        copyValidatedData();
    }
    if (_validateCallback) {
        return _validateCallback(*this);
    }
    return isValid();
}

bool Form::validateOnly()
{
    _hasChanged = false;
    _errors.clear();
    for (const auto &field: _fields) {
        // __DBG_printf("name=%s disabled=%u", field->getName().c_str(), field->isDisabled());
        if (field->getType() != FormField::Type::GROUP && field->isDisabled() == false) {
            if (_data->hasArg(field->getName())) {
                __LDBG_printf("Form::validateOnly() Set value %s = %s", field->getName().c_str(), _data->arg(field->getName()).c_str());
                if (field->setValue(_data->arg(field->getName()))) {
                    _hasChanged = true;
                }
                if (field->hasValidators()) {
                    for (const auto &validator: field->getValidators()) {
                        if (!validator->validate()) {
                            _errors.emplace_back(field.get(), validator->getMessage());
                        }
                    }
                }
            }
            else if (_invalidMissing) {
                _errors.emplace_back(field.get(), FSPGM(Form_value_missing_default_message));
            }
        }
    }
#if DEBUG_KFC_FORMS
    dump(DEBUG_OUTPUT, "Form Dump: ");
#endif
    return isValid();
}

bool Form::isValid() const
{
    return _errors.empty();
}

bool Form::hasChanged() const
{
    return _hasChanged;
}

bool Form::hasError(FormField &field) const
{
    for (const auto &error: _errors) {
        if (error.is(field)) {
            return true;
        }
    }
    return false;
}

void Form::copyValidatedData()
{
    if (isValid()) {
        for (const auto &field : _fields) {
            if (field->isDisabled() == false) {
                field->copyValue();
            }
        }
    }
}

const Form::ErrorsVector &Form::getErrors() const
{
    return _errors;
}

void Form::finalize() const
{
#if DEBUG_KFC_FORMS
    dump(DEBUG_OUTPUT, "Form Dump: ");
#endif
}

bool Form::process(const String &name, Print &output)
{
    for (const auto &field : _fields) {
        auto len = field->getName().length();
        if (field->getType() == FormField::Type::TEXT && name.equalsIgnoreCase(field->getName())) {
            __LDBG_printf("name=%s text=%s", name.c_str(), field->getValue().c_str());
            PrintHtmlEntities::printTo(PrintHtmlEntities::Mode::ATTRIBUTE, field->getValue().c_str(), output);
            return true;
        }
        else if (field->getType() == FormField::Type::TEXTAREA && name.equalsIgnoreCase(field->getName())) {
            __LDBG_printf("name=%s textarea=%s", name.c_str(), field->getValue().c_str());
            PrintHtmlEntities::printTo(PrintHtmlEntities::Mode::HTML, field->getValue().c_str(), output);
            return true;
        }
        else if (field->getType() == FormField::Type::CHECK && name.equalsIgnoreCase(field->getName())) {
            __LDBG_printf("name=%s checkbox=%d", name.c_str(), field->getValue().toInt());
            if (field->getValue().toInt()) {
                output.print(FSPGM(_checked, " checked"));
            }
            return true;
        }
        else if (field->getType() == FormField::Type::SELECT && strncasecmp(field->getName().c_str(), name.c_str(), len) == 0) {
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

const char *Form::jsonEncodeString(const String &str, PrintInterface &output)
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

void Form::createJavascript(PrintInterface &output)
{
    if (!isValid()) {
        __LDBG_printf(PSTR("errors=%d"), _errors.size());
        output.printf_P(PSTR("<script>" FORMUI_CRLF "$.formValidator.addErrors("));
        uint16_t idx = 0;
        for (auto &error: _errors) {
            output.printf_P(PSTR("%c{'target':'#%s','error':'%s'}"), idx++ ? ',' : '[', jsonEncodeString(error.getName(), output), jsonEncodeString(error.getMessage(), output));
        }
        output.printf_P(PSTR("]);" FORMUI_CRLF "</script>"));
    }
}

void Form::dump(Print &out, const String &prefix) const {

    out.print(prefix);
    out.println(F("Errors:"));
    if (_errors.empty()) {
        out.print(prefix);
        out.println(FSPGM(None, "None"));
    }
    else {
        for (auto &error : _errors) {
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

FormData *Form::getFormData() const
{
    return _data;
}

void Form::setValidateCallback(ValidateCallback validateCallback)
{
    _validateCallback = validateCallback;
}
