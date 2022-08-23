/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if MDNS_PLUGIN

// Wrapper for service discovery

#include <Arduino_compat.h>

class MDNSService {
public:
    static bool addService(const String &service, const String &proto, uint16_t port);
    static bool addServiceTxt(const String &service, const String &proto, const String &key, const String &value);
    static bool removeService(const String &service, const String &proto);
    static bool removeServiceTxt(const String &service, const String &proto, const String &key);
    #if ENABLE_ARDUINO_OTA
        static void enableArduino();
    #endif
    static void announce();
};

#endif
