/**
* Author: sascha_lammers@gmx.de
*/

#include "FormData.h"

#if !FORM_DATA_CLASS_OVERRIDE

FormData::FormData() 
{
}

FormData::~FormData() 
{
}

void FormData::clear() 
{
    _data.clear();
}

const String FormData::arg(const String &name) const 
{
    auto iterator = _data.find(name);
    if (iterator != _data.end()) {
        return iterator->second;
    }
    return String();
}

bool FormData::hasArg(const String &name) const 
{
    return _data.find(name) != _data.end();
}

void FormData::set(const String &name, const String &value) 
{
    _data[name] = value;
}

#endif
