/**
* Author: sascha_lammers@gmx.de
*/

#include "FormCallbackValidator.h"
#include "FormField.h"
#include "Form.h"

FormCallbackValidator::FormCallbackValidator(Callback callback) : FormCallbackValidator(String(), callback)
{
}

FormCallbackValidator::FormCallbackValidator(const String & message, Callback callback) : FormValidator(message), _callback(callback)
{
}

bool FormCallbackValidator::validate()
{
    return FormValidator::validate() && _callback(getField().getValue(), getField());
}
