/**
  Author: sascha_lammers@gmx.de
*/

#ifdef ESP32

#pragma once

#include <Arduino_compat.h>
#include <HTTPClient.h>
#include <BufferStream.h>
#include <LoopFunctions.h>

class asyncHTTPrequest : public HTTPClient {
public:
    asyncHTTPrequest() : _dataCb(nullptr), _dataCbArg(nullptr), _readyStateCb(nullptr), _readyStateCbArg(nullptr) {
    }

    typedef std::function<void(void *, asyncHTTPrequest *, int readyState)> readyStateChangeCB;
    typedef std::function<void(void *, asyncHTTPrequest *, size_t available)> onDataCB;

    bool open(const __FlashStringHelper *method, const char *url) {
        _method = method;
        begin(url);
        return true;
    }

    bool send(const String &payload = String()) {
        _payload = payload;
        sendRequest(_method.c_str(), payload);
        //dump(Serial);
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
        return _returnCode;
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
    String _method;
    String _payload;
    BufferStream _body;
};

#endif
