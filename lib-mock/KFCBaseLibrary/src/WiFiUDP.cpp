/**
* Author: sascha_lammers@gmx.de
*/

#if _WIN32

#include <Arduino_compat.h>

WiFiUDP::WiFiUDP() {
    init_winsock();
    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    _buffer = nullptr;
    _clear();
}

WiFiUDP::~WiFiUDP() {
    closesocket(_socket);
    free(_buffer);
}

int WiFiUDP::beginPacket(const char *host, uint16_t port) {
    IPAddress address;
    if (!WiFi.hostByName(host, address)) {
        return 0;
    }
    _dst.sin_addr.S_un.S_addr = (uint32_t)address;
    return _beginPacket(port);
}

int WiFiUDP::beginPacket(IPAddress ip, uint16_t port) {
    _dst.sin_addr.S_un.S_addr = (uint32_t)ip;
    return 1;
}

int WiFiUDP::_beginPacket(uint16_t port) {
    _clear();
    _buffer = (char *)malloc(0xffff);
    if (!_buffer) {
        return 0;
    }
    _dst.sin_family = AF_INET;
    _dst.sin_port = htons(port);
    return 1;
}

size_t WiFiUDP::write(uint8_t byte) {
    if (_buffer && _len < 0xffff) {
        _buffer[_len++] = byte;
        return 1;
    }
    return 0;
}

size_t WiFiUDP::write(const uint8_t *buffer, size_t size) {
    const uint8_t *ptr = buffer;
    while (size--) {
        if (write(*ptr) != 1) {
            break;
        }
        ptr++;
    }
    return ptr - buffer;
}

void WiFiUDP::flush() {
    endPacket();
}

int WiFiUDP::endPacket() {
    if (_socket == INVALID_SOCKET || !_buffer || !_dst.sin_family) {
        return 0;
    }

    int result = sendto(_socket, _buffer, _len, 0, (SOCKADDR *)&_dst, sizeof(_dst));
    _clear();

    if (result == SOCKET_ERROR) {
        return 0;
    }
    return 1;
}

void WiFiUDP::_clear() {
    _len = 0;
    _dst.sin_family = 0;
    free(_buffer);
    _buffer = nullptr;
}

#endif
