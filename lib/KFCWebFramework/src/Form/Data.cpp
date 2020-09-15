/**
* Author: sascha_lammers@gmx.de
*/

#ifndef HAVE_KFC_FIRMWARE_VERSION

#include "Form/Data.h"

using namespace FormUI::Form;

Data::Data()
{
}

Data::~Data()
{
}

void Data::clear()
{
    _data.clear();
}

const String Data::arg(const String &name) const
{
    auto iterator = _data.find(name);
    if (iterator != _data.end()) {
        return iterator->second;
    }
    return String();
}

bool Data::hasArg(const String &name) const
{
    return _data.find(name) != _data.end();
}

void Data::setValue(const String &name, const String &value)
{
    _data[name] = value;
}

#endif
