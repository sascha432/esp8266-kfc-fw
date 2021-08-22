/**
 * Author: sascha_lammers@gmx.de
 */

#if MDNS_PLUGIN

#include <Arduino_compat.h>
#include "mdns_plugin.h"
#include "mdns_resolver.h"

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

#if ESP8266

    #include <lwip/dns.h>

    IPAddress MDNSResolver::MDNSServiceInfo::findIP4Address(const IPAddress &myAddress)
    {
        IPAddress address;
        IPAddress closestAddress;
        uint16_t cntIP4Adress = p_pMDNSResponder.answerIP4AddressCount(p_hServiceQuery, p_u32AnswerIndex);
        for (uint32_t u2 = 0; u2 < cntIP4Adress; ++u2)
        {
            address = p_pMDNSResponder.answerIP4Address(p_hServiceQuery, p_u32AnswerIndex, u2);
            if (myAddress[0] == address[0] && myAddress[1] == address[1] && myAddress[2] == address[2]) {
                closestAddress = address;
                break;
            }
            else if (myAddress[0] == address[0] && myAddress[1] == address[1]) {
                closestAddress = address;
            }
            __LDBG_printf("address=%s my_address=%s closest=%s", address.toString().c_str(), myAddress.toString().c_str(), closestAddress.toString().c_str());
        }
        auto &result = IPAddress_isValid(closestAddress) ? closestAddress : address;
        __LDBG_printf("found=%s", result.toString().c_str());
        return result;
    };

    char *MDNSResolver::MDNSServiceInfo::findTxtValue(const String &key)
    {
        for (auto kv = p_pMDNSResponder._answerKeyValue(p_hServiceQuery, p_u32AnswerIndex); kv != nullptr; kv = kv->m_pNext) {
            __LDBG_printf("find_key=%s key=%s value=%s", key.c_str(), kv->m_pcKey, kv->m_pcValue);
            if (key.equals(kv->m_pcKey)) {
                return kv->m_pcValue;
            }
        }
        return nullptr;
    }

    void MDNSResolver::Query::serviceCallback(bool map, MDNSResolver::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent)
    {
        __LDBG_printf("answerType=%u p_bSetContent=%u", answerType, p_bSetContent);
        PrintString str;

        mdnsServiceInfo.serviceDomain();

        if (mdnsServiceInfo.hostDomainAvailable() && _isAddress) {
            __LDBG_printf("domain=%s", mdnsServiceInfo.hostDomain());
            ip_addr_t addr;
            auto result = dns_gethostbyname(mdnsServiceInfo.hostDomain(), &addr, (dns_found_callback)&dnsFoundCallback, this); // try to resolve before marking as collected
            if (result == ERR_OK) {
                _hostname = IPAddress(addr.addr).toString();
                _dataCollected |= DATA_COLLECTED_HOSTNAME;
            }
            else if (result == ERR_INPROGRESS) {
                // waiting for callback
            }
            __LDBG_printf("dns_gethostbyname=%d this=%p", result, this);
        }
        if (mdnsServiceInfo.IP4AddressAvailable() && _isAddress) {
            _address = mdnsServiceInfo.findIP4Address(WiFi.localIP());
            _dataCollected |= DATA_COLLECTED_ADDRESS;
        }
        if (mdnsServiceInfo.hostPortAvailable() && _isPort) {
            __LDBG_printf("port=%u", mdnsServiceInfo.hostPort());
            _port = mdnsServiceInfo.hostPort();
            _dataCollected |= DATA_COLLECTED_PORT;
        }
        if (mdnsServiceInfo.txtAvailable()) {
            if (!_isAddress) {
                auto value = mdnsServiceInfo.findTxtValue(_addressValue);
                if (value) {
                    if (IPAddress_isValid(_address = convertToIPAddress(value))) {
                        _dataCollected |= DATA_COLLECTED_ADDRESS;
                    } else {
                        _hostname = value;
                        _dataCollected |= DATA_COLLECTED_HOSTNAME;
                    }
                }
            }
            if (!_isPort) {
                auto value = mdnsServiceInfo.findTxtValue(_portValue);
                if (value) {
                    _port = static_cast<uint16_t>(atoi(value));
                    _dataCollected |= DATA_COLLECTED_PORT;
                }
            }
        }

        __LDBG_printf("data_collected=%u port=%u address_or_host=%u callback=%u", _dataCollected, (_dataCollected & DATA_COLLECTED_PORT), (_dataCollected & (DATA_COLLECTED_ADDRESS|DATA_COLLECTED_HOSTNAME)), (bool)_callback);

        // collected address or hostname and port
        if ((_dataCollected & DATA_COLLECTED_PORT) && (_dataCollected & (DATA_COLLECTED_ADDRESS|DATA_COLLECTED_HOSTNAME)) && !_resolved) {
            _resolved = true;
            end();
        }
    }

#elif ESP32

    // void MDNSResolver::Query::serviceCallback(bool map, MDNSResolver::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent)
    // {
    //     __LDBG_printf("answerType=%u p_bSetContent=%u", answerType, p_bSetContent);
    //     PrintString str;

    //     mdnsServiceInfo.serviceDomain();

    //     if (mdnsServiceInfo.hostDomainAvailable() && _isAddress) {
    //         __LDBG_printf("domain=%s", mdnsServiceInfo.hostDomain());
    //         ip_addr_t addr;
    //         auto result = dns_gethostbyname(mdnsServiceInfo.hostDomain(), &addr, (dns_found_callback)&dnsFoundCallback, this); // try to resolve before marking as collected
    //         if (result == ERR_OK) {
    //             _hostname = IPAddress(addr.addr).toString();
    //             _dataCollected |= DATA_COLLECTED_HOSTNAME;
    //         }
    //         else if (result == ERR_INPROGRESS) {
    //             // waiting for callback
    //         }
    //         __LDBG_printf("dns_gethostbyname=%d this=%p", result, this);
    //     }
    //     if (mdnsServiceInfo.IP4AddressAvailable() && _isAddress) {
    //         _address = mdnsServiceInfo.findIP4Address(WiFi.localIP());
    //         _dataCollected |= DATA_COLLECTED_ADDRESS;
    //     }
    //     if (mdnsServiceInfo.hostPortAvailable() && _isPort) {
    //         __LDBG_printf("port=%u", mdnsServiceInfo.hostPort());
    //         _port = mdnsServiceInfo.hostPort();
    //         _dataCollected |= DATA_COLLECTED_PORT;
    //     }
    //     if (mdnsServiceInfo.txtAvailable()) {
    //         if (!_isAddress) {
    //             auto value = mdnsServiceInfo.findTxtValue(_addressValue);
    //             if (value) {
    //                 if (IPAddress_isValid(_address = convertToIPAddress(value))) {
    //                     _dataCollected |= DATA_COLLECTED_ADDRESS;
    //                 } else {
    //                     _hostname = value;
    //                     _dataCollected |= DATA_COLLECTED_HOSTNAME;
    //                 }
    //             }
    //         }
    //         if (!_isPort) {
    //             auto value = mdnsServiceInfo.findTxtValue(_portValue);
    //             if (value) {
    //                 _port = static_cast<uint16_t>(atoi(value));
    //                 _dataCollected |= DATA_COLLECTED_PORT;
    //             }
    //         }
    //     }

    //     __LDBG_printf("data_collected=%u port=%u address_or_host=%u callback=%u", _dataCollected, (_dataCollected & DATA_COLLECTED_PORT), (_dataCollected & (DATA_COLLECTED_ADDRESS|DATA_COLLECTED_HOSTNAME)), (bool)_callback);

    //     // collected address or hostname and port
    //     if ((_dataCollected & DATA_COLLECTED_PORT) && (_dataCollected & (DATA_COLLECTED_ADDRESS|DATA_COLLECTED_HOSTNAME)) && !_resolved) {
    //         _resolved = true;
    //         end();
    //     }
    // }

#endif

MDNSResolver::Query::Query(const String &name, const String &service, const String &proto, const String &addressValue, const String &portValue, const String &fallback, uint16_t port, const String &prefix, const String &suffix, ResolvedCallback callback, uint16_t timeout) :
    _name(name),
    _endTime(END_TIME_NOT_STARTED),
    _service(service),
    _proto(proto),
    _addressValue(addressValue),
    _portValue(portValue),
    _fallback(fallback),
    _prefix(prefix),
    _suffix(suffix),
    _callback(callback),
    _serviceQuery(nullptr),
    _port(port),
    _fallbackPort(port),
    _timeout(timeout),
    _dataCollected(DATA_COLLECTED_NONE),
    _resolved(false),
    _isAddress(addressValue.equals(FSPGM(address))),
    _isPort(portValue.equals(FSPGM(port)))
{
}

MDNSResolver::Query::~Query()
{
    end();
}

void MDNSResolver::Query::begin()
{
    __LDBG_printf("service_query=%p zeroconf=%s callback=%u", _serviceQuery, createZeroConfString().c_str(), (bool)_callback);
    if (_endTime == END_TIME_FINISHED) {
        __LDBG_printf("end time set to finished");
        return;
    }
    if (!_callback) {
        __LDBG_printf("callback already removed");
        return;
    }
    if (_serviceQuery) {
        __LDBG_printf("service query already set");
        return;
    }
    _endTime = millis() + _timeout;
    if (_endTime == END_TIME_FINISHED) {
        __LDBG_printf("unlucky day");
        _endTime--;
    } else if (_endTime == END_TIME_NOT_STARTED) {
        __LDBG_printf("unlucky day");
        _endTime++;
    }

    #if ESP8266
        _serviceQuery = MDNS.installServiceQuery(_service.c_str(), _proto.c_str(), [this](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
            __LDBG_printf("info=%p answer_type=%u content=%u", &mdnsServiceInfo, answerType, p_bSetContent);
            auto &resolverInfo = static_cast<MDNSResolver::MDNSServiceInfo &>(mdnsServiceInfo);
            serviceCallback(true, resolverInfo, answerType, p_bSetContent);
        });
    #endif
}


void MDNSResolver::Query::end()
{
    __LDBG_printf("end_time=%u dur=%d resolved=%u this=%p", _endTime, millis() - (_endTime - _timeout), _resolved, this);
    if (_endTime != END_TIME_FINISHED) {
        _endTime = END_TIME_FINISHED;

        // run in main loop
        LoopFunctions::callOnce([this]() {
            __LDBG_printf("main loop end=%p", this);
            if (_serviceQuery) {
                __LDBG_printf("service_query=%p zeroconf=%s", _serviceQuery, createZeroConfString().c_str());
                #if ESP8266
                    MDNS.removeServiceQuery(_serviceQuery);
                #endif
                _serviceQuery = nullptr;
            }

            bool logging = _name.length() && System::Device::getConfig().zeroconf_logging;
            if (_resolved) {
                if (logging) {
                    Logger_notice(F("%s: Zeroconf response %s:%u"), _name.c_str(), IPAddress_isValid(_address) ? _address.toString().c_str() : _hostname.c_str(), _port);
                }
                _prefix += IPAddress_isValid(_address) ? _address.toString() : _hostname;
                _prefix += _suffix;
                _callback(_hostname, _address, _port, _prefix, ResponseType::RESOLVED);
            }
            else {
                if (logging) {
                    Logger_notice(F("%s: Zeroconf fallback %s:%u"), _name.c_str(), _fallback.c_str(), _port);
                }
                _prefix += _fallback;
                _prefix += _suffix;
                _callback(_fallback, convertToIPAddress(_fallback), _fallbackPort, _prefix, ResponseType::TIMEOUT);
            }
            MDNSPlugin::removeQuery(this);
        });
    }
}

void MDNSResolver::Query::dnsFoundCallback(const char *name, const ip_addr *ipaddr, void *arg)
{
    __LDBG_printf("dnsFoundCallback=%p query=%p address=%s", arg, MDNSPlugin::getPlugin().findQuery(arg), ipaddr ? IPAddress(ipaddr->addr).toString().c_str() : SPGM(null));
    if (ipaddr && MDNSPlugin::getPlugin().findQuery(arg)) { // verify that the query has not been deleted yet
        auto &query = *reinterpret_cast<MDNSResolver::Query *>(arg);
        #if ESP32
            auto addr = ipaddr->u_addr.ip4.addr;
        #else
            auto addr = ipaddr->addr;
        #endif
        query._hostname = IPAddress(addr).toString();
        query._dataCollected |= DATA_COLLECTED_HOSTNAME;
        __LDBG_printf("resolved=%s", query._hostname.c_str());
    }
}

MDNSResolver::Query::StateType MDNSResolver::Query::getState() const
{
    return _endTime == END_TIME_FINISHED ? StateType::FINISHED : (_endTime == END_TIME_NOT_STARTED ? StateType::NONE : StateType::STARTED);
}

void MDNSResolver::Query::checkTimeout()
{
    if ((_endTime != END_TIME_FINISHED) && (_endTime != END_TIME_NOT_STARTED)) {
        if (millis() >= _endTime) {
            end();
        }
    }
}

void MDNSResolver::Query::createZeroConf(Print &output) const
{
    output.print(F("${zeroconf:"));
    output.print(_service);
    output.print('.');
    output.print(_proto);
    output.print(',');
    output.print(_addressValue);
    //if (!_portValue.equals(FSPGM(port)))
    {
        output.print(':');
        output.print(_portValue);
    }
    if (_fallback.length()) {
        output.print('|');
        output.print(_fallback);
    }
    output.print('}');
}

String MDNSResolver::Query::createZeroConfString() const
{
    PrintString str;
    createZeroConf(str);
    return str;
}

#endif
