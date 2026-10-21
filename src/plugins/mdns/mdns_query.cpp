/**
 * Author: sascha_lammers@gmx.de
 */

// ESP8266 mDNS service discovery with bounded memory
//
// The service query of the core's mDNS responder (MDNS.installServiceQuery()) keeps every answer with its
// domains, TXT items and addresses on the heap. It allocates with a plain "new" in the network context and the
// core aborts on a failed allocation, so 10 devices on the network rebooted the ESP8266 (~20 KB, OOM).
//
// This query is sent from an ephemeral UDP port. RFC 6762 6.7 ("legacy unicast"): the responders answer a
// query that does not come from port 5353 with one unicast packet to the sender, which contains the PTR, SRV,
// TXT and A records. Each packet is parsed into the JSON output of MDNSPlugin::Output and released right away,
// only the hash of the service names that were already printed is kept.

#if MDNS_PLUGIN && ESP8266

#include <Arduino_compat.h>
#include <JsonTools.h>
#include <lwip/udp.h>
#include <lwip/pbuf.h>
#include <crc16.h>
#include "mdns_plugin.h"
#include "mdns_def.h"

#if DEBUG_MDNS_SD
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace KFCJson;

namespace {

    constexpr uint16_t kMdnsPort = 5353;
    constexpr uint16_t kTypeA = 1;
    constexpr uint16_t kTypePTR = 12;
    constexpr uint16_t kTypeTXT = 16;
    constexpr uint16_t kTypeSRV = 33;
    constexpr uint16_t kHeaderSize = 12;
    constexpr uint8_t kMaxRecords = 32;
    constexpr uint8_t kMaxAddresses = 4;
    constexpr uint8_t kMaxNameLength = 64; // longer names are truncated
    constexpr uint8_t kMaxTxtLength = 32;  // longer TXT values are truncated
    constexpr uint8_t kSendCount = 2;      // the query is sent twice, lost packets are common on WiFi

    // the TXT keys of the KFC firmware that the discovery page shows (version, build, title, device)
    const char kTxtKeys[] PROGMEM = "vbtd";

    inline uint16_t read16(pbuf *p, uint16_t offset)
    {
        return (pbuf_get_at(p, offset) << 8) | pbuf_get_at(p, offset + 1);
    }

    // reads a domain name at offset and follows compression pointers. out receives the dotted name (truncated
    // to outSize - 1), out can be nullptr to skip the name. returns the offset behind the name in the record or
    // 0 for an invalid name
    uint16_t readName(pbuf *p, uint16_t offset, char *out, size_t outSize)
    {
        const uint16_t length = p->tot_len;
        uint16_t next = 0;
        size_t pos = 0;
        uint8_t jumps = 0;
        if (out && outSize) {
            out[0] = 0;
        }
        while (offset < length) {
            uint8_t labelLength = pbuf_get_at(p, offset);
            if (labelLength == 0) {
                return next ? next : offset + 1;
            }
            if ((labelLength & 0xc0) == 0xc0) {
                if (offset + 1 >= length || ++jumps > 16) {
                    return 0;
                }
                if (!next) {
                    next = offset + 2;
                }
                offset = ((labelLength & 0x3f) << 8) | pbuf_get_at(p, offset + 1);
                continue;
            }
            if (labelLength & 0xc0) {
                return 0; // reserved label type
            }
            offset++;
            if (offset + labelLength > length) {
                return 0;
            }
            if (out && outSize) {
                if (pos && pos + 1 < outSize) {
                    out[pos++] = '.';
                }
                for (uint8_t i = 0; i < labelLength && pos + 1 < outSize; i++) {
                    out[pos++] = pbuf_get_at(p, offset + i);
                }
                out[pos] = 0;
            }
            offset += labelLength;
        }
        return 0;
    }

    void printEscaped(Print &output, const char *str)
    {
        JsonTools::Utf8Buffer buffer;
        JsonTools::printToEscaped(output, str, strlen(str), &buffer);
    }

    void udpReceive(void *arg, udp_pcb *pcb, pbuf *p, const ip_addr_t *addr, u16_t port)
    {
        (void)pcb;
        (void)addr;
        if (p) {
            reinterpret_cast<MDNSPlugin::Output *>(arg)->_onPacket(p, port);
            pbuf_free(p);
        }
    }

}

bool MDNSPlugin::Output::begin(const char *service, const char *proto)
{
    // every packet needs its own pbuf: lwIP leaves the UDP header it added in the pbuf, a second udp_sendto()
    // with the same pbuf sends a malformed mDNS packet to every device (the core's responder allocated its
    // RDLength unchecked and rebooted, capped in lib/ESP8266mDNS 1.2.0-kfc2)
    // query name _<service>._<proto>.local, the leading underscores are optional like for MDNS.installServiceQuery()
    char name[kMaxNameLength];
    snprintf_P(name, sizeof(name), PSTR("%s%s.%s%s.local"), *service == '_' ? "" : "_", service, *proto == '_' ? "" : "_", proto);

    // header + labels + terminating 0 + type + class
    uint8_t query[kHeaderSize + kMaxNameLength + 6] = {};
    _queryId = static_cast<uint16_t>(os_random() | 1);
    query[0] = _queryId >> 8;
    query[1] = _queryId & 0xff;
    query[5] = 1; // one question
    auto ptr = query + kHeaderSize;
    auto label = name;
    while (*label) {
        auto dot = strchr(label, '.');
        size_t labelLength = dot ? dot - label : strlen(label);
        *ptr++ = labelLength;
        memcpy(ptr, label, labelLength);
        ptr += labelLength;
        label += labelLength + (dot ? 1 : 0);
    }
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = kTypePTR;
    *ptr++ = 0;
    *ptr++ = 1; // class IN
    uint16_t length = ptr - query;

    _pcb = udp_new();
    if (!_pcb || udp_bind(_pcb, IP_ADDR_ANY, 0) != ERR_OK) {
        _end();
        return false;
    }
    udp_recv(_pcb, udpReceive, this);
    // the multicast interface is the station
    ip4_addr_t stationAddress;
    ip4_addr_set_u32(&stationAddress, static_cast<uint32_t>(WiFi.localIP()));
    udp_set_multicast_netif_addr(_pcb, &stationAddress);

    ip_addr_t mdnsAddress;
    IP_ADDR4(&mdnsAddress, 224, 0, 0, 251);
    err_t err = ERR_OK;
    for (uint8_t i = 0; i < kSendCount && err == ERR_OK; i++) {
        // a new buffer for each packet, the WiFi driver may still hold the previous one
        auto p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
        if (!p) {
            err = ERR_MEM;
            break;
        }
        memcpy(p->payload, query, length);
        err = udp_sendto(_pcb, p, &mdnsAddress, kMdnsPort);
        pbuf_free(p);
    }
    if (err != ERR_OK) {
        _end();
        return false;
    }
    return true;
}

void MDNSPlugin::Output::_end()
{
    if (_pcb) {
        udp_recv(_pcb, nullptr, nullptr);
        udp_remove(_pcb);
        _pcb = nullptr;
    }
}

void MDNSPlugin::Output::_onPacket(pbuf *p, uint16_t port)
{
    if (!_timeout || port != kMdnsPort || p->tot_len < kHeaderSize) {
        return;
    }
    if (ESP.getFreeHeap() < MDNS_DISCOVERY_STOP_FREE_HEAP) {
        __LDBG_printf("free heap %u, answer dropped", ESP.getFreeHeap());
        return;
    }
    // response with the id of the query
    if (read16(p, 0) != _queryId || (pbuf_get_at(p, 2) & 0x80) == 0) {
        return;
    }
    uint16_t questions = read16(p, 4);
    uint16_t records = read16(p, 6) + read16(p, 8) + read16(p, 10);
    if (records > kMaxRecords) {
        records = kMaxRecords;
    }

    uint16_t offset = kHeaderSize;
    while (questions--) {
        if ((offset = readName(p, offset, nullptr, 0)) == 0) {
            return;
        }
        offset += 4;
    }

    char service[kMaxNameLength] = {};
    char host[kMaxNameLength] = {};
    char txt[sizeof(kTxtKeys) - 1][kMaxTxtLength] = {};
    uint32_t addresses[kMaxAddresses];
    uint8_t addressCount = 0;
    uint16_t hostPort = 0;

    while (records--) {
        if ((offset = readName(p, offset, nullptr, 0)) == 0 || offset + 10 > p->tot_len) {
            return;
        }
        uint16_t type = read16(p, offset);
        uint16_t rdLength = read16(p, offset + 8);
        uint16_t rdata = offset + 10;
        offset = rdata + rdLength;
        if (offset > p->tot_len) {
            return;
        }
        switch (type) {
            case kTypePTR:
                readName(p, rdata, service, sizeof(service));
                break;
            case kTypeSRV:
                if (rdLength > 6) {
                    hostPort = read16(p, rdata + 4);
                    readName(p, rdata + 6, host, sizeof(host));
                }
                break;
            case kTypeA:
                if (rdLength == 4 && addressCount < kMaxAddresses) {
                    uint32_t address;
                    pbuf_copy_partial(p, &address, 4, rdata);
                    addresses[addressCount++] = address;
                }
                break;
            case kTypeTXT: {
                    // <length><key>=<value> ...
                    uint16_t pos = rdata;
                    while (pos < offset) {
                        uint8_t itemLength = pbuf_get_at(p, pos++);
                        if (pos + itemLength > offset) {
                            break;
                        }
                        if (itemLength >= 2 && pbuf_get_at(p, pos + 1) == '=') {
                            // a NUL key would match the terminator of kTxtKeys = one row behind txt[]
                            const char key = pbuf_get_at(p, pos);
                            auto keyPtr = key ? strchr_P(kTxtKeys, key) : nullptr;
                            if (keyPtr) {
                                auto &value = txt[keyPtr - kTxtKeys];
                                uint8_t valueLength = std::min<uint8_t>(itemLength - 2, kMaxTxtLength - 1);
                                pbuf_copy_partial(p, value, valueLength, pos + 2);
                                value[valueLength] = 0;
                            }
                        }
                        pos += itemLength;
                    }
                }
                break;
            default:
                break;
        }
    }

    if (!*service || !*host) {
        return; // incomplete answer
    }
    // the query is sent twice and every responder answers both
    uint32_t hash = crc16_update(~0, service, strlen(service));
    if (std::find(_answered.begin(), _answered.end(), hash) != _answered.end()) {
        return;
    }

    MUTEX_LOCK_BLOCK(_lock) {
        if (!_timeout) {
            return;
        }
        _answered.push_back(hash);
        next();
        _output.print(F("\"s\":\""));
        printEscaped(_output, service);
        _output.print(F("\",\"h\":\""));
        printEscaped(_output, host);
        _output.printf_P(PSTR("\",\"p\":%u,\"a\":["), hostPort);
        for (uint8_t i = 0; i < addressCount; i++) {
            _output.print('"');
            IPAddress(addresses[i]).printTo(_output);
            _output.print(F("\","));
        }
        StrWrapper(_output).rtrim(',');
        _output.print(F("],"));
        for (uint8_t i = 0; i < sizeof(kTxtKeys) - 1; i++) {
            if (*txt[i]) {
                _output.printf_P(PSTR("\"%c\":\""), pgm_read_byte(kTxtKeys + i));
                printEscaped(_output, txt[i]);
                _output.print(F("\","));
            }
        }
    }

    // flush a pending response right away. _lock must not be held here, _fillBuffer() takes the same lock
    if (_notify) {
        _notify();
    }
}

#endif
