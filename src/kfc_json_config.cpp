/**
* Author: sascha_lammers@gmx.de
*/

#if HAVE_ARDUINO_JSON_CFG

#include <Arduino_compat.h>
#include <ArduinoJson.h>
#include <PrintString.h>
#include "kfc_json_config.h"

#if DEBUG_KFC_JSON_CONFIG
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

KFCJsonConfig::KFCJsonConfig() :
    _jsonDoc(0),
    _modified(false)
{
}

bool KFCJsonConfig::begin()
{
    if (_modified) {
        __LDBG_printf("modified state set, calling end");
        end();
    }
    __LDBG_printf("begin");
    _modified = false;
    _file = KFCFS.open(PrintString(F("/.cfg/kfc_config.json")), FileOpenMode::writeplus);
    if (_file) {
        // read config from file
        _jsonDoc = DynamicJsonDocument(2048);
        deserializeJson(_jsonDoc, _file);
        // deserializeMsgPack(_jsonDoc, _file);
        return true;
    }
    return false;
}

void KFCJsonConfig::end()
{
    __LDBG_printf("end modified=%u", _modified);
    if (_modified) {
        // save changes
        _file.truncate(0);
        serializeJson(_jsonDoc, _file);
    }
    _modified = false;
    _file.close();
    _jsonDoc.clear();
}

void KFCJsonConfig::erase()
{
    __LDBG_printf("end modified=%u", _modified);
    _jsonDoc.clear();
    _modified = false;
    _file.truncate(0);
    _file.close();
}

void KFCJsonConfig::setString(const __FlashStringHelper *name, const __FlashStringHelper *value)
{
    __LDBG_printf("setString(%s)=%s", __S(name), __S(value));
    _jsonDoc[name] = value;
    _modified = true;
}

void KFCJsonConfig::setBlob(const __FlashStringHelper *name, const uint8_t *value, size_t len)
{
    __LDBG_printf("setBlob(%s, %u)=%s", __S(name), len);
    // deserializeMsgPack()

    // _jsonDoc[name] =

}

#endif
