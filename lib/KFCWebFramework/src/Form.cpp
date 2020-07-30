/**
* Author: sascha_lammers@gmx.de
*/

#include "Form.h"
#include "FormBase.h"
#include "FormData.h"
#include "FormError.h"
#include "FormValidator.h"
#include <JsonTools.h>

#if DEBUG_KFC_FORMS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Form::Form(FormData *data) : _data(data), _invalidMissing(true), _hasChanged(false)
{
}

Form::~Form()
{
    clearForm();
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
    group.setFormUI(type, label);
    return group;
}

FormField &Form::_add(FormField *field)
{
    field->setForm(this);
    _fields.emplace_back(field);
    return *field;
}

FormValidator &Form::addValidator(int index, FormValidator &&validator)
{
    return _fields.at(index)->addValidator(std::move(validator));
}

FormValidator &Form::addValidator(FormValidator &&validator)
{
    return _fields.back()->addValidator(std::move(validator));
}

FormValidator &Form::addValidator(FormField &field, FormValidator &&validator)
{
    return field.addValidator(std::move(validator));
}

FormUI::UI &Form::addFormUI(FormUI::UI &&formUI)
{
    return addFormUI(new FormUI::UI(std::move(formUI)));
}

FormUI::UI &Form::addFormUI(FormUI::UI *formUI)
{
    _fields.back()->setFormUI(formUI);
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
        if (field->getType() != FormField::Type::GROUP) {
            if (_data->hasArg(field->getName())) {
                _debug_printf_P(PSTR("Form::validateOnly() Set value %s = %s\n"), field->getName().c_str(), _data->arg(field->getName()).c_str());
                if (field->setValue(_data->arg(field->getName()))) {
                    _hasChanged = true;
                }
                for (const auto &validator: field->getValidators()) {
                    if (!validator->validate()) {
                        _errors.emplace_back(field.get(), validator->getMessage());
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
            field->copyValue();
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

const char *Form::process(const String &name) const
{
    // TODO check html entities encoding
    for (const auto &field : _fields) {
        uint8_t len = (uint8_t)field->getName().length();
        if (field->getType() == FormField::Type::TEXT && name.equalsIgnoreCase(field->getName())) {
            _debug_printf_P(PSTR("Form::process(%s) INPUT_TEXT = %s\n"), name.c_str(), field->getValue().c_str());
            return field->getValue().c_str();
        }
        else if (field->getType() == FormField::Type::CHECK && name.equalsIgnoreCase(field->getName())) {
            _debug_printf_P(PSTR("Form::process(%s) INPUT_CHECK = %d\n"), name.c_str(), field->getValue().toInt());
            return field->getValue().toInt() ? " checked" : "";
        }
        else if (field->getType() == FormField::Type::SELECT && strncasecmp(field->getName().c_str(), name.c_str(), len) == 0) {
            if (name.length() == len) {
                _debug_printf_P(PSTR("Form::process(%s) INPUT_SELECT = %s\n"), name.c_str(), field->getValue().c_str());
                return field->getValue().c_str();
            }
            else if (name.charAt(len) == '_') {
                int value = name.substring(len + 1).toInt();
                if (value == field->getValue().toInt()) {
                    _debug_printf_P(PSTR("Form::process(%s) INPUT_SELECT %d = %d\n"), name.c_str(), value, (int)field->getValue().toInt());
                    return " selected";
                }
                else {
                    _debug_printf_P(PSTR("Form::process(%s) INPUT_SELECT %d != %d\n"), name.c_str(), value, (int)field->getValue().toInt());
                    return "";
                }
            }
        }
    }
    _debug_printf_P(PSTR("Form::process(%s) not found\n"), name.c_str());
    return nullptr;
}

const char *Form::_jsonEncodeString(const String &str, PrintInterface &output)
{
    if (JsonTools::lengthEscaped(str) == str.length()) {
        return str.c_str();
    }
    else {
        PrintString encoded;
        // encodes the name and attached it to the PrintInterface
        JsonTools::printToEscaped(encoded, str);
        return output.attachString(std::move(encoded));
    }
}

void Form::createJavascript(PrintInterface &output) const
{
    if (!isValid()) {
        __LDBG_printf(PSTR("errors=%d"), _errors.size());
        output.printf_P(PSTR("<script>" FORMUI_CRLF "$.formValidator.addErrors("));
        uint16_t idx = 0;
        for (auto &error: _errors) {
            output.printf_P(PSTR("%c{'target':'#%s','error':'%s'}"), idx++ ? ',' : '[', _jsonEncodeString(error.getName(), output), _jsonEncodeString(error.getMessage(), output));
        }
        output.printf_P(PSTR("]);" FORMUI_CRLF "</script>"));
    }
}

void Form::setFormUI(const String &title, const String &submit)
{
    _formTitle = title;
    _formSubmit = submit;
}

void Form::setFormUI(const String &title)
{
    _formTitle = title;
}

void Form::createHtml(PrintInterface &output)
{
    if (_formTitle.length()) {
        output.printf_P(PSTR("<h1>%s</h1>" FORMUI_CRLF), _formTitle.c_str());
    }
    for (const auto &field : _fields) {
        field->html(output);
    }
    PGM_P label = _formSubmit.length() ? _formSubmit.c_str() : SPGM(Save_Changes, "Save Changes");
    output.printf_P(PSTR("<button type=\"submit\" class=\"btn btn-primary\">%s...</button>" FORMUI_CRLF), label);
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
