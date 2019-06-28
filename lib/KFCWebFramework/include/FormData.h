/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#if defined(FORM_DATA_CLASS_OVERRIDE)

// alternative storage class

typedef std::function<const String(const String &name)> FormDataArgCallback_t;
typedef std::function<bool(const String &name)> FormDataHasArgCallback_t;

class FormData {
public:
    void setCallbacks(FormDataArgCallback_t arg, FormDataHasArgCallback_t hasArg) {
        _arg = arg;
        _hasArg = hasArg;
    }

    const String arg(const String &name) const {
        return _arg(name);
    }
    bool hasArg(const String &name) const {
        return _hasArg(name);
    }

private:
    FormDataArgCallback_t _arg;
    FormDataHasArgCallback_t _hasArg;
};


#else

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

#endif
