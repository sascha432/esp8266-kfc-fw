/**
* Author: sascha_lammers@gmx.de
*/

#include "Form.h"
#include "FormBase.h"
#include "FormData.h"
#include "FormError.h"
#include "FormValidator.h"

#if DEBUG_KFC_FORMS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Form::Form() : _data(nullptr), _invalidMissing(true)
{
}

Form::Form(FormData * data) : Form()
{
    _data = data;
}

Form::~Form()
{
    clearForm();
}

void Form::clearForm()
{
    for (auto field : _fields) {
        delete field;
    }
    _fields.clear();
    _errors.clear();
}

void Form::setFormData(FormData * data)
{
    _data = data;
}

void Form::setInvalidMissing(bool invalidMissing)
{
    _invalidMissing = invalidMissing;
}

int Form::add(FormField *field)
{
    field->setForm(this);
    _fields.push_back(field);
    return _fields.size() - 1;
}

FormField *Form::_add(FormField *field)
{
    add(field);
    return field;
}

FormField *Form::add(const String &name, const String &value, FormField::FieldType_t type)
{
    return _add(new FormField(name, value, type));
}

FormField *Form::add(const String &name, const String &value, FormStringObject::SetterCallback_t setter, FormField::FieldType_t type)
{
    return _add(new FormStringObject(name, value, setter, type));
}

FormField *Form::add(const String &name, String *value, FormField::FieldType_t type)
{
    return _add(new FormStringObject(name, value, type));
}

FormValidator *Form::addValidator(int index, FormValidator *validator)
{
    _fields.at(index)->addValidator(validator);
    return validator;
}

FormValidator *Form::addValidator(FormValidator * validator)
{
    _fields.back()->addValidator(validator);
    return validator;
}

FormValidator *Form::addValidator(const String &name, FormValidator * validator)
{
    auto field = getField(name);
    if (field) {
        field->addValidator(validator);
        return validator;
    }
    else {
        delete validator;
    }
    return nullptr;
}

FormField *Form::getField(const String &name) const {
    for (auto field : _fields) {
        if (field->getName().equals(name)) {
            return field;
        }
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
    return isValid();
}

bool Form::validateOnly()
{
    _hasChanged = false;
    _errors.clear();
    for (auto field : _fields) {
        if (_data->hasArg(field->getName())) {
            _debug_printf_P(PSTR("Form::validateOnly() Set value %s = %s\n"), field->getName().c_str(), _data->arg(field->getName()).c_str());
            if (field->setValue(_data->arg(field->getName()))) {
                _hasChanged = true;
            }
            for (auto validator : field->getValidators()) {
                if (!validator->validate()) {
                    _errors.push_back(FormError(field, validator->getMessage()));
                }
            }
        }
        else if (_invalidMissing) {
            _errors.push_back(FormError(field, FSPGM(Form_value_missing_default_message)));
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

bool Form::hasError(FormField *field) const
{
    for (const auto &error : _errors) {
        if (error.is(field)) {
            return true;
        }
    }
    return false;
}

void Form::copyValidatedData()
{
    if (isValid()) {
        for (auto field : _fields) {
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
    for(auto field : _fields) {
        uint8_t len = (uint8_t)field->getName().length();
        if (field->getType() == FormField::INPUT_TEXT && name.equalsIgnoreCase(field->getName())) {
            _debug_printf_P(PSTR("Form::process(%s) INPUT_TEXT = %s\n"), name.c_str(), field->getValue().c_str());
            return field->getValue().c_str();
        }
        else if (field->getType() == FormField::INPUT_CHECK && name.equalsIgnoreCase(field->getName())) {
            _debug_printf_P(PSTR("Form::process(%s) INPUT_CHECK = %d\n"), name.c_str(), field->getValue().toInt());
            return field->getValue().toInt() ? " checked" : "";
        }
        else if (field->getType() == FormField::INPUT_SELECT && strncasecmp(field->getName().c_str(), name.c_str(), len) == 0) {
            if (name.length() == len) {
                _debug_printf_P(PSTR("Form::process(%s) INPUT_SELECT = %s\n"), name.c_str(), field->getValue().c_str());
                return field->getValue().c_str();
            }
            else if (name.charAt(len) == '_') {
                int value = name.substring(len + 1).toInt();
                if (value == field->getValue().toInt()) {
                    _debug_printf_P(PSTR("Form::process(%s) INPUT_SELECT %d = %d\n"), name.c_str(), value, field->getValue().toInt());
                    return " selected";
                }
                else {
                    _debug_printf_P(PSTR("Form::process(%s) INPUT_SELECT %d != %d\n"), name.c_str(), value, field->getValue().toInt());
                    return "";
                }
            }
        }
    }
    _debug_printf_P(PSTR("Form::process(%s) not found\n"), name.c_str());
    return nullptr;
}

void Form::createJavascript(Print &out)
{
    if (!isValid()) {
        _debug_printf_P(PSTR("Form::createJavascript(): errors=%d\n"), _errors.size());
        out.print(F("<script>\n$.formValidator.addErrors(["));
        uint8_t idx = 0;
        for(const auto &error : _errors) {
            if (idx++) {
                out.print(',');
            }
            out.printf_P(PSTR("{'target':'#%s','error':'%s'}"), error.getName().c_str(), error.getMessage().c_str()); //TODO escape quotes etc
        }
        out.println(F("]);\n</script>"));
    }
}

void Form::setFormUI(const String& title, const String& submit)
{
    _formTitle = title;
    _formSubmit = submit;
}

void Form::setFormUI(const String& title)
{
    _formTitle = title;
}

void Form::createHtml(Print& output)
{
    if (_formTitle.length()) {
        output.printf_P(PSTR("<h1>%s</h1>"), _formTitle.c_str());
    }
    for (auto field : _fields) {
        field->html(output);
    }
    PGM_P label = _formSubmit.length() ? _formSubmit.c_str() : PSTR("Save Changes");
    output.printf_P(PSTR("<button type=\"submit\" class=\"btn btn-primary\">%s...</button>"), label );
}

void Form::createHtmlPart(Print& output, uint16_t num)
{
    if (num == 0) {
        if (_formTitle.length()) {
            output.printf_P(PSTR("<h1>%s</h1>"), _formTitle.c_str());
        }
    }
    else if (num >= 1 && num <= _fields.size()) {
        _fields.at(num - 1)->html(output);
    }
    else if (num == _fields.size() + 1) {
        PGM_P label = _formSubmit.length() ? _formSubmit.c_str() : PSTR("Save Changes");
        output.printf_P(PSTR("<button type=\"submit\" class=\"btn btn-primary\">%s...</button>"), label);
    }
}

void Form::dump(Print &out, const String &prefix) const {

    out.print(prefix);
    out.println(F("Errors:"));
    if (_errors.empty()) {
        out.print(prefix);
        out.println(F("None"));
    } else {
        for(const auto &error : _errors) {
            out.print(prefix);
            out.print(error.getName());
            out.print(F(": "));
            out.println(error.getMessage());
        }
    }
    out.print(prefix);
    if (_hasChanged) {
        out.println(F("Form data was modified"));
    } else {
        out.println(F("Form data was not modified"));
    }
    out.print(prefix);
    out.println(F("Form data:"));
    for(auto field : _fields) {
        out.print(prefix);
        out.print(field->getName());
        if (field->hasChanged()) {
            out.print('*');
        }
        if (hasError(field)) {
            out.print('#');
        }
        out.print(F(": '"));
        out.print(field->getValue());
        out.println('\'');
    }
}
