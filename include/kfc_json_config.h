/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <ArduinoJson.h>

class KFCJsonConfig
{
public:
    KFCJsonConfig();

    bool begin(const __FlashStringHelper *subConfig);
    void end();

private:
    DynamicJsonDocument _json;
    File _file;
    char _configType;
    bool _modified;

};
