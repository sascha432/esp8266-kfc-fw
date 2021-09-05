/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include <EventScheduler.h>
#include <misc.h>
#include "mdns_plugin.h"
#include "mdns_sd.h"

#if MDNS_PLUGIN

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

bool MDNSService::addService(const String &service, const String &proto, uint16_t port)
{
    __LDBG_printf("service=%s proto=%s port=%u running=%u", service.c_str(), proto.c_str(), port, MDNSPlugin::getInstance()._isRunning());
    if (!MDNSPlugin::getInstance()._isRunning()) {
        return false;
    }
    #if ESP8266
        auto result = MDNS.addService(service, proto, port);
        __LDBG_printf("result=%u", result);
        return result;
    #else
        MDNS.addService(service, proto, port);
        return true;
    #endif
}

bool MDNSService::addServiceTxt(const String &service, const String &proto, const String &key, const String &value)
{
    __LDBG_printf("service=%s proto=%s key=%s value=%s running=%u", service.c_str(), proto.c_str(), key.c_str(), value.c_str(), MDNSPlugin::getInstance()._isRunning());
    if (!MDNSPlugin::getInstance()._isRunning()) {
        return false;
    }
    #if ESP8266
        auto result = MDNS.addServiceTxt(service, proto, key, value);
        __LDBG_printf("result=%u", result);
        return result;
    #else
        MDNS.addServiceTxt(service, proto, key, value);
        return true;
    #endif
}

bool MDNSService::removeService(const String &service, const String &proto)
{
    __LDBG_printf("service=%s proto=%s running=%u", service.c_str(), proto.c_str(), MDNSPlugin::getInstance()._isRunning());
    if (!MDNSPlugin::getInstance()._isRunning()) {
        return false;
    }
    #if ESP8266
        auto result = MDNS.removeService(nullptr, service.c_str(), proto.c_str());
        __LDBG_printf("result=%u", result);
        return result;
    #else
        return mdns_service_remove(service.c_str(), proto.c_str()) == ESP_OK;
    #endif
}

bool MDNSService::removeServiceTxt(const String &service, const String &proto, const String &key)
{
    __LDBG_printf("service=%s proto=%s key=%s running=%u", service.c_str(), proto.c_str(), key.c_str(), MDNSPlugin::getInstance()._isRunning());
    if (!MDNSPlugin::getInstance()._isRunning()) {
        return false;
    }
    #if ESP8266
        auto result = MDNS.removeServiceTxt(nullptr, service.c_str(), proto.c_str(), key.c_str());
        __LDBG_printf("result=%u", result);
        return result;
    #else
        return mdns_service_txt_item_remove(service.c_str(), proto.c_str(), key.c_str()) == ESP_OK;
    #endif
}


#else

bool MDNSService::addService(const String &service, const String &proto, uint16_t port)
{
    return false;
}

bool MDNSService::addServiceTxt(const String &service, const String &proto, const String &key, const String &value)
{
    return false;
}

bool MDNSService::removeService(const String &service, const String &proto)
{
    return false;
}

bool MDNSService::removeServiceTxt(const String &service, const String &proto, const String &key)
{
    return false;
}

void MDNSService::announce()
{
}

#endif
