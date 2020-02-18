/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "BufferStream.h"
#include "LoopFunctions.h"

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

protected:
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


class asyncHTTPrequest : public HTTPClient {
public:
    asyncHTTPrequest() : _dataCb(nullptr), _dataCbArg(nullptr), _readyStateCb(nullptr), _readyStateCbArg(nullptr) {
    }

    typedef std::function<void(void *, asyncHTTPrequest *, int readyState)> readyStateChangeCB;
    typedef std::function<void(void *, asyncHTTPrequest *, size_t available)> onDataCB;

    bool open(const char *method, const char *url) {
        _method = method;
        begin(url);
        return true;
    }

    bool send(const String &payload = String()) {
        _payload = payload;
        _request();
        dump(Serial);
        LoopFunctions::callOnce([this]() {
            loop();
        });
        return true;
    }

    size_t responseRead(uint8_t *buffer, size_t len) {
        return _body.readBytes(buffer, len);
    }

    size_t available() {
        return _body.available();
    }

    void abort() {
        _body = BufferStream();
    }

    void setReqHeader(const String &name, const String &value) {
        addHeader(name, value);
    }

    void onData(onDataCB cb, void *arg = nullptr) {
        _dataCb = cb;
        _dataCbArg = arg;
    }

    void onReadyStateChange(readyStateChangeCB cb, void *arg = nullptr) {
        _readyStateCb = cb;
        _readyStateCbArg = arg;
    }

    int responseHTTPcode() {
        if (_error.length()) {
            return -1;
        }
        return _httpCode;
    }

    void loop() {
        if (_readyStateCb) {
            _readyStateCb(_readyStateCbArg, this, 1);
            _readyStateCb(_readyStateCbArg, this, 2);
            _readyStateCb(_readyStateCbArg, this, 3);
        }
        while (available()) {
            if (_dataCb) {
                _dataCb(_dataCbArg, this, available());
            }
        }
        if (_readyStateCb) {
            _readyStateCb(_readyStateCbArg, this, 4);
        }
        end();
    }

private:
    onDataCB _dataCb;
    void *_dataCbArg;
    readyStateChangeCB _readyStateCb;
    void *_readyStateCbArg;
};
