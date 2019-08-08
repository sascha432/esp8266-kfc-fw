/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class HTTPClient {
public:
    HTTPClient();
    virtual ~HTTPClient();

    void begin(String url);
    void end();

    int GET();
    size_t getSize();
    Stream &getStream();

private:
    void _close();

    SOCKET _socket;
    String _url;
    String _host;
    uint16_t _port;
    String _path;
    BufferStream *_body;
    int _httpCode;
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
