/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "Types.h"
#include "Utility/Debug.h"

namespace FormUI {

    namespace Field {

        class BaseField {
        public:
            BaseField(const char *name, const String &value = String(), InputFieldType type = InputFieldType::NONE) :
                _name(name),
                _value(value),
                _formUI(nullptr),
                _form(nullptr),
                _type(type),
                _hasChanged(false),
                _disabled(false),
                _expanded(false),
                _groupOpen(false)
            {
            }

            BaseField(const char *name, InputFieldType type = InputFieldType::NONE) :
                BaseField(name, String(), type)
            {
            }

            virtual ~BaseField();

            // call in the constructor of any child to intialize the value
            /**
            * Initialize the value of the field. Should only be used in the constructor.
            **/
            void initValue(const String &value)
            {
                _value = value;
                _hasChanged = false;
            }

            // parent form and render setttings
            //
            void setForm(Form::BaseForm *form);
            Form::BaseForm &getForm() const;

            WebUI::Config &getWebUIConfig();

            ///
            // compare name operators and other name related methods
            ///
            bool operator==(const String &name) const {
                return strcmp_P(name.c_str(), _name) == 0;
            }
            bool operator==(const __FlashStringHelper *name) const {
                return strcmp_P_P((PGM_P)name, _name) == 0;
            }
            bool operator==(const char *name) const {
                return strcmp_P_P(name, _name) == 0;
            }

            PGM_P getName() const;

            // returns ltrim '#.' of the name
            PGM_P getNameForType() const;

            // returns '" id="' if name starts with '#' or ' ' if it starts with '.', and an empty string if the name is empty
            // use with printf("... class=\"row%s%s\" ...", getNameType(),getNameForType());
            PGM_P getNameType() const;

            // return value stored in base field
            const String &getValue() const;

            // set value of the field and return true, if it has changed
            // children may call this method to assign the value rather than setting it directly (though its protected not private)
            virtual bool setValue(const String &value);

            /*
            * This method is called when the user submits a valid form. The validated data is stored
            * in the value field as a string and can be retried using getValue()
            **/
            virtual void copyValue();

            // compared value to another field
            bool equals(Field::BaseField *field) const;

            // returns true if value has changed
            bool hasChanged() const;

            // mark value as changed
            void setChanged(bool hasChanged);

            // set type of input field
            void setType(InputFieldType type);

            // get type of input field
            InputFieldType getType() const;

            // get render type
            RenderType getRenderType() const;

            // construct BaseUI and attach to field
            template <typename... Args>
            WebUI::BaseUI &setFormUI(Field::BaseField *parent, Args &&... args)  {
                if (_formUI) {
                    delete _formUI;
                }
                _formUI = new WebUI::BaseUI(parent, std::forward<Args>(args)...);
                return *_formUI;
            }

            // add or replace BaseUI attached to the field
            WebUI::BaseUI &setFormUI(WebUI::BaseUI *formUI);

            // return BaseUI attached to this field or nullptr
            WebUI::BaseUI *getFormUI() const;

            // disabled fields are not validated or copied
            // they do not have to be present in the POST data
            void setDisabled(bool state);
            bool isDisabled() const;

            void html(PrintInterface &output);

        protected:
            friend Form::BaseForm;

            const char *_name;
            String _value;
            WebUI::BaseUI *_formUI;
            Form::BaseForm *_form;
            InputFieldType _type;
            bool _hasChanged : 1;
            bool _disabled: 1;
        protected:
            bool _expanded: 1;
            bool _groupOpen : 1;
        };

    }

}

#include "BaseField.hpp"

#include <debug_helper_disable.h>
