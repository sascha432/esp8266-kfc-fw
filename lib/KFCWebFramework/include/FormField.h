/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "FormUI.h"


class Form;
class FormValidator;

class FormField {
public:
    typedef std::vector <FormValidator *> ValidatorsVector;

    using PrintInterface = PrintArgs::PrintInterface;

    enum class InputFieldType {
        NONE = 0,
        CHECK,
        SELECT,
        TEXT,
        GROUP,
    };

    FormField(const String &name, const String &value = String(), InputFieldType type = FormField::InputFieldType::NONE);
    // FormField(const String &name, const String &value, bool optional) : FormField(name, value) {
    //     _optional = optional;
    // }
    virtual ~FormField();

    void setForm(Form *form);
    Form &getForm() const;

    // void setOptional(bool optional) {
    //     _optional = optional;
    // }
    // const bool isOptional() const {
    //     return _optional;
    // }

    const String &getName() const;

    /**
    * Returns the value of the initialized field or changes the user submitted
    **/
    const String &getValue();

    /**
    * Initialize the value of the field. Should only be used in the constructor.
    **/
    void initValue(const String &value);

    /**
    * This method is called when the user submits a form
    **/
    virtual bool setValue(const String &value);

    /*
    * This method is called when the user submits a valid form. The validated data is stored
    * in the value field as a string and can be retried using getValue()
    **/
    virtual void copyValue();

    bool equals(FormField *field) const;

    bool hasChanged() const;
    void setChanged(bool hasChanged);

    void setType(InputFieldType type);
    InputFieldType getType() const;

    void setFormUI(FormUI *formUI);
    void html(PrintInterface &output);

    void addValidator(FormValidator *validator);
    const ValidatorsVector &getValidators() const;

private:
    friend Form;

    String _name;
    String _value;
    ValidatorsVector _validators;
    InputFieldType _type;
    FormUI *_formUI;
    Form *_form;
    bool _hasChanged;
    // bool _optional;
    // bool _notSet;
};

class FormGroup : public FormField {
public:
    FormGroup(const FormGroup &group) = delete;
    FormGroup(const String &name, bool expanded) : FormField(name, String(), FormField::InputFieldType::GROUP), _expanded(expanded) {
    }
    virtual bool setValue(const String &value) override {
        return false;
    }
    virtual void copyValue() override {
    }
    void end(FormUI::TypeEnum_t type = FormUI::TypeEnum_t::GROUP_END);
    void endDiv();

    bool isExpanded() const {
        return _expanded;
    }
private:
    bool _expanded;
};
