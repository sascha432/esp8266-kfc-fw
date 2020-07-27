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
    auto &result = closestAddress.isSet() ? closestAddress : address;
    __LDBG_printf("found=%s", result.toString().c_str())
    return result;
};

char *MDNSResolver::MDNSServiceInfo::findTxtValue(const String &key)
{
    for (auto kv = p_pMDNSResponder._answerKeyValue(p_hServiceQuery, p_u32AnswerIndex); kv != nullptr; kv = kv->m_pNext)
    {
        __LDBG_printf("find_key=%s key=%s value=%s", key.c_str(), kv->m_pcKey, kv->m_pcValue);
        if (key.equals(kv->m_pcKey)) {
            return kv->m_pcValue;
        }
    }
    return nullptr;
}



MDNSResolver::Query::Query(const String &service, const String &proto, const String &addressValue, const String &portValue, const String &fallback, uint16_t port, ResolvedCallback callback, uint16_t timeout) :
    _endTime(END_TIME_NOT_STARTED),
    _dataCollected(DATA_COLLECTED_NONE),
    _service(service),
    _proto(proto),
    _addressValue(addressValue),
    _portValue(portValue),
    _fallback(fallback),
    _fallbackPort(port),
    _port(port),
    _callback(callback),
    _timeout(timeout),
    _serviceQuery(nullptr),
    _resolved(false),
    _isAddress(String_equals(addressValue, PSTR("address"))),
    _isDomain(String_equals(addressValue, PSTR("domain"))),
    _isPort(String_equals(portValue, PSTR("port")))
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

    _serviceQuery = MDNS.installServiceQuery(_service.c_str(), _proto.c_str(), [this](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
        __LDBG_printf("info=%p answer_type=%u content=%u", &mdnsServiceInfo, answerType, p_bSetContent);
        auto &resolverInfo = static_cast<MDNSResolver::MDNSServiceInfo &>(mdnsServiceInfo);
        serviceCallback(true, resolverInfo, answerType, p_bSetContent);
    });
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
                MDNS.removeServiceQuery(_serviceQuery);
                _serviceQuery = nullptr;
            }

            if (_resolved) {
                _callback(_hostname, _address, _port, ResponseType::RESOLVED);
            }
            else {
                _callback(_fallback, convertToIPAddress(_fallback), _fallbackPort, ResponseType::TIMEOUT);
            }
            MDNSPlugin::removeQuery(this);
        });
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
    //if (!String_equals(_portValue, SPGM(port)))
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

void MDNSResolver::Query::serviceCallback(bool map, MDNSResolver::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent)
{
    __LDBG_printf("answerType=%u p_bSetContent=%u", answerType, p_bSetContent)
    PrintString str;

    mdnsServiceInfo.serviceDomain();

    if (mdnsServiceInfo.hostDomainAvailable() && _isDomain) {
        __LDBG_printf("domain=%s", mdnsServiceInfo.hostDomain());
        _hostname = mdnsServiceInfo.hostDomain();
        _dataCollected |= DATA_COLLECTED_HOSTNAME;
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
        if (!_isAddress && !_isDomain) {
            auto value = mdnsServiceInfo.findTxtValue(_addressValue);
            if (value) {
                if ((_address = convertToIPAddress(value)).isSet()) {
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
                _port = (uint16_t)atoi(value);
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

#endif
