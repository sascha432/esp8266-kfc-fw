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
    using PrintInterface = PrintArgs::PrintInterface;

    using FormValidatorPtr = std::unique_ptr<FormValidator>;
    using ValidatorsVector = std::vector<FormValidatorPtr>;

    enum class Type {
        NONE = 0,
        CHECK,
        SELECT,
        TEXT,
        GROUP,
    };

    FormField(const String &name, const String &value = String(), Type type = Type::NONE);
    // FormField(const String &name, const String &value, bool optional) : FormField(name, value) {
    //     _optional = optional;
    // }
    virtual ~FormField();

    void setForm(Form *form);
    Form &getForm() const;

    bool operator==(const String &name) const {
        return _name.equals(name);
    }
    bool operator==(const __FlashStringHelper *name) const {
        return String_equals(_name, name);
    }

    // void setOptional(bool optional) {
    //     _optional = optional;
    // }
    // const bool isOptional() const {
    //     return _optional;
    // }

    const String &getName() const;

    // returns ltrim '#.' of the name
    const char *getNameForType() const;

    // returns '" id="' if name starts with '#' or ' ' if it starts with '.', and an empty string if the name is empty
    // use with printf("... class=\"row%s%s\" ...", getNameType(),getNameForType());
    PGM_P getNameType() const;

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

    void setType(Type type);
    Type getType() const;

    template <typename... Args>
    FormUI::UI &setFormUI(Args &&... args) {
        return setFormUI(new FormUI::UI(std::forward<Args>(args)...));
    }

    FormUI::UI &setFormUI(FormUI::UI *formUI);

    FormUI::Type getFormType() const;
    void html(PrintInterface &output);

    FormValidator &addValidator(FormValidator &&validator);
    ValidatorsVector &getValidators();

private:
    friend Form;

    String _name;
    String _value;
    ValidatorsVector _validators;
    Type _type;
    FormUI::UI *_formUI;
    Form *_form;
    bool _hasChanged;
    // bool _optional;
    // bool _notSet;
};

class FormGroup : public FormField {
public:
    FormGroup(const FormGroup &group) = delete;
    FormGroup(const String &name, bool expanded) : FormField(name, String(), Type::GROUP), _expanded(expanded) {
    }
    virtual bool setValue(const String &value) override {
        return false;
    }
    virtual void copyValue() override {
    }
    void end();

    bool isExpanded() const {
        return _expanded;
    }
private:
    bool _expanded;
};
