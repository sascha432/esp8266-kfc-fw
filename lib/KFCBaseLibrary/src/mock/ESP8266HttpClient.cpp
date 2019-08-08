/**
* Author: sascha_lammers@gmx.de
*/

#if _WIN32

#include "BufferStream.h"
#include "ESP8266HttpClient.h"

HTTPClient::HTTPClient() {
    init_winsock();
    _body = _debug_new BufferStream();
    _httpCode = 0;
}

HTTPClient::~HTTPClient() {
    _close();
    delete _body;
}

void HTTPClient::begin(String url) {
    _url = url;
    if (!_url.startsWith("http://")) {
        printf("Only http:// is supported\n");
        exit(-1);
    }
    _host = _url.substring(7);
    int pos = _host.indexOf('/');
    if (pos != -1) {
        _path = _host.substring(pos);
        _host.remove(pos);
    }
    pos = _host.indexOf(':');
    if (pos != -1) {
        _port = (uint16_t)_host.substring(pos + 1).toInt();
        _host.remove(pos);
    }
    else {
        _port = 80;
    }
}

void HTTPClient::end() {
    _body->clear();
    _url = String();
    _host = String();
    _path = String();
}

int HTTPClient::GET() {
    sockaddr_in addr;
    char buffer[1024];
    int iResult;

    _httpCode = 0;
    _body->clear();

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    struct hostent *dns;
    if (!(dns = gethostbyname(_host.c_str()))) {
        return 0;
    }
    if (dns->h_addrtype != AF_INET) {
        return 0;
    }
    addr.sin_addr.S_un.S_addr = *(u_long *)dns->h_addr_list[0];

    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_socket == INVALID_SOCKET) {
        return 0;
    }

    iResult = connect(_socket, (SOCKADDR *)&addr, sizeof(addr));
    if (iResult == SOCKET_ERROR) {
        _close();
        return WSAGetLastError();
    }

    if (_port != 80) {
        snprintf(buffer, sizeof(buffer), "GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n\r\n", _path.c_str(), _host.c_str(), _port);
    } else {
        snprintf(buffer, sizeof(buffer), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", _path.c_str(), _host.c_str());
    }
    iResult = send(_socket, buffer, strlen(buffer), 0);
    if (iResult == SOCKET_ERROR) {
        _close();
        return WSAGetLastError();
    }

    iResult = shutdown(_socket, 1);
    if (iResult == SOCKET_ERROR) {
        _close();
        return WSAGetLastError();
    }

    bool isHeader = true;
    String header;

    do {
        iResult = recv(_socket, buffer, sizeof(buffer) - 1, 0);
        if (iResult > 0) {
            buffer[iResult] = 0;
            if (isHeader) {
                int size = 2;
                char *ptr = strstr(buffer, "\n\n"); // TODO this fails if \n\n was truncated during the last read (recv)
                if (!ptr) {
                    size = 4;
                    ptr = strstr(buffer, "\r\n\r\n");
                }
                if (!ptr) {
                    header += buffer;
                } else {
                    isHeader = false;
                    *ptr = 0;
                    header += buffer;
                    _body->write((const uint8_t *)ptr + size, iResult - (ptr - buffer + size));
                }
            } else {
                _body->write((const uint8_t *)buffer, iResult);
            }
        } else if (iResult < 0) {
            _close();
            return WSAGetLastError();
        }
    } while( iResult > 0 );
    _close();

    if (!header.startsWith("HTTP/1")) {
        _httpCode = 500;
        return _httpCode;
    }

    int pos = header.indexOf(' ');
        if (pos == -1) {
        _httpCode = 500;
        return _httpCode;
    }
    _httpCode = header.substring(pos + 1).toInt();

    return _httpCode;
}

size_t HTTPClient::getSize() {
    return _body->length();
}

Stream &HTTPClient::getStream() {
    return *_body;
}

void HTTPClient::_close()
{
    if (_socket != INVALID_SOCKET) {
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
}

#endif
