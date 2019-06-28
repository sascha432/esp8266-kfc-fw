/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <map>

class FormData {
public:
    FormData();
    virtual ~FormData();

    void clear();

    const String arg(const String &name) const;
    bool hasArg(const String &name) const;

    void set(const String &name, const String &value);

private:
    std::map<String, String> _data;
};

