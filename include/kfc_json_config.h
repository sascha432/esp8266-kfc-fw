/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if HAVE_ARDUINO_JSON_CFG

#include <Arduino_compat.h>
#include <ArduinoJson.h>

#ifndef DEBUG_KFC_JSON_CONFIG
#    define DEBUG_KFC_JSON_CONFIG (1 || defined(DEBUG_ALL))
#endif

#if DEBUG_KFC_JSON_CONFIG
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

class KFCJsonConfig
{
public:
    KFCJsonConfig();

    bool begin();
    void end();
    void erase();

    void setString(const __FlashStringHelper *name, const __FlashStringHelper *value);
    void setString(const __FlashStringHelper *name, const String &value)
    {
         setString(name, reinterpret_cast<const __FlashStringHelper *>(value.c_str()));
    }
    void setString(const __FlashStringHelper *name, PGM_P value)
    {
         setString(name, reinterpret_cast<const __FlashStringHelper *>(value));
    }

    void setBool(const __FlashStringHelper *name, bool value);
    void setInt(const __FlashStringHelper *name, int64_t value);
    void setUint(const __FlashStringHelper *name, uint64_t value);

    void setBlob(const __FlashStringHelper *name, const uint8_t *value, size_t len);

private:
    DynamicJsonDocument _jsonDoc;
    File _file;
    bool _modified;

};

#if DEBUG_KFC_JSON_CONFIG
#    include <debug_helper_disable.h>
#endif

#endif
