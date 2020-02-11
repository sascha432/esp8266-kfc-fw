/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonMapReader.h"

class HTTPClient {
public:
    HTTPClient();
    virtual ~HTTPClient();

    void begin(String url);
    void end();

    int GET();
    int POST(const String &payload);
    size_t getSize();
    Stream &getStream();
    void addHeader(const String& name, const String& value, bool first = false, bool replace = true);
    void setAuthorization(const char* auth);

    void dump(Print& output, int maxLen = 500);

    void setTimeout(uint16_t timeout) {
        _timeout = timeout;
    }

    BufferStream& getBufferStream() {
        return _body;
    }

private:
    class HttpHeader {
    public:
        HttpHeader(const String& _name, const String& _value = String()) : name(_name), value(_value) {
        }
        bool operator==(const HttpHeader& header) {
            return name == header.name;
        }
        String name;
        String value;
    };
    typedef std::function<int()> Callback_t;

    int _request();
    int _readSocket(char* buffer, size_t size);
    int _getLastError();
    int _waitFor(Callback_t callback);
    void _close();

    SOCKET _socket;
    String _method;
    String _payload;
    String _url;
    String _host;
    String _error;
    uint16_t _port;
    String _path;
    BufferStream _body;
    String _headers;
    std::vector<HttpHeader> _headersList;
    PrintString _requestHeaders;
    int _httpCode;
    uint16_t _timeout;
    uint32_t _endTime;
    int _lastError;
};

struct rst_info{
    uint32_t reason;
    uint32_t exccause;
    uint32_t epc1;
    uint32_t epc2;
    uint32_t epc3;
    uint32_t excvaddr;
    uint32_t depc;
};
