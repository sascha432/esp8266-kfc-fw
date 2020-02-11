/**
* Author: sascha_lammers@gmx.de
*/

#if _WIN32

#include <Arduino_compat.h>
#include "WiFi.h"

ESP8266WiFiClass WiFi;

int ESP8266WiFiClass::hostByName(const char* aHostname, IPAddress &aResult, uint32_t timeout_ms)
{
    struct addrinfo* result = NULL;
    struct addrinfo* ptr = NULL;
    struct addrinfo hints {};

    if (aResult.fromString(aHostname)) {
        return true;
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int iResult = getaddrinfo(aHostname, "0", &hints, &result);
    if (iResult != 0) {
        return -1;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        if (ptr->ai_family == AF_INET) {
            aResult = (uint32_t)((struct sockaddr_in*)ptr->ai_addr)->sin_addr.S_un.S_addr;
            return true;
        }
    }
    return false;
}

#endif