/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#pragma comment(lib, "Ws2_32.lib")

class WiFiUDP : public Stream {
public:
    WiFiUDP();
    ~WiFiUDP();

    int beginPacket(const char *host, uint16_t port);
    int beginPacket(IPAddress ip, uint16_t port);
    size_t write(uint8_t byte);
    size_t write(const uint8_t *buffer, size_t size);
    void flush();
    int endPacket();

private:
    int _beginPacket(uint16_t port);
    void _clear();

    int available() override {
        return 0;
    }
    int read() override {
        return -1;
    }
    int peek() override {
        return -1;
    }


private:
    SOCKET _socket;
    sockaddr_in _dst;
    char *_buffer;
    uint16_t _len;
};

