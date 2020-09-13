/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#ifdef HAVE_KFC_FIRMWARE_VERSION

#include <ESPAsyncWebServer.h>

class FormData : public AsyncWebServerRequest {
public:
    using AsyncWebServerRequest::AsyncWebServerRequest;
    bool hasArg(const String &name) const {
       return AsyncWebServerRequest::hasArg(name.c_str());
    }
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

    void setValue(const String &name, const String &value);

private:
    std::map<String, String> _data;
};

#endif
