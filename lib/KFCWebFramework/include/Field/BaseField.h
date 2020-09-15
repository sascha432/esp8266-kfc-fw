/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "Types.h"

namespace FormUI {

    namespace Field {

        class BaseField {
        public:
            using Type = ::FormUI::Field::Type;

            BaseField(const String &name, const String &value = String(), Type type = Type::NONE);
            BaseField(const String &name, Type type = Type::NONE) : BaseField(name, String(), type) {}
            virtual ~BaseField();

            void setForm(Form::BaseForm *form);
            Form::BaseForm &getForm() const;
            WebUI::Config &getWebUIConfig();

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
            const String &getValue() const;

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

            bool equals(Field::BaseField *field) const;

            bool hasChanged() const;
            void setChanged(bool hasChanged);

            void setType(Type type);
            Type getType() const;

            template <typename... Args>
            WebUI::BaseUI &setFormUI(Field::BaseField *parent, Args &&... args) {
                return setFormUI(new WebUI::BaseUI(parent, std::forward<Args>(args)...));
            }

            WebUI::BaseUI &setFormUI(WebUI::BaseUI *formUI);
            WebUI::BaseUI *getFormUI() const {
                return _formUI;
            }

            WebUI::Type getFormType() const;
            void html(PrintInterface &output);

            void setDisabled(bool state) {
                _disabled = state;
            }

            bool isDisabled() const {
                return _disabled;
            }

        protected:
            friend Form::BaseForm;

            String _name;
            String _value;
            WebUI::BaseUI *_formUI;
            Form::BaseForm *_form;
            Type _type;
            bool _hasChanged : 1;
            bool _disabled: 1;
        protected:
            bool _expanded: 1;
        };

    }

}
