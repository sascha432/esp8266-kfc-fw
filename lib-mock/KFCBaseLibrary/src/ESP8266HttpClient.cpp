/**
* Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#include <PrintString.h>
#include <misc.h>
#include "BufferStream.h"
#include "ESP8266HttpClient.h"

HTTPClient::HTTPClient() {
    init_winsock();
    _httpCode = 0;
    _timeout = 30;
    _lastError = 0;
}

HTTPClient::~HTTPClient() {
    _close();
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
    *this = HTTPClient();    
}

int HTTPClient::POST(const String& payload)
{
    _method = "POST";
    _payload = payload;
    return _request();
}

int HTTPClient::GET()
{
    _method = "GET";
    _payload = String();
    return _request();
}

int HTTPClient::_readSocket(char* buffer, size_t size)
{
    int len;
    do {
        len = recv(_socket, buffer, size, 0);
        if (len == WSAEWOULDBLOCK && millis() > _endTime) {
            _error = "Timeout";
            return -1;
        }
    } while (len == WSAEWOULDBLOCK);
    
    if (len < 0) {
        return _getLastError();
    }
    return len;
}

int HTTPClient::_getLastError()
{
    int err = WSAGetLastError();
    char msgbuf[256] = { 0 };
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgbuf, sizeof(msgbuf), NULL);
    if (!*msgbuf) {
        sprintf(msgbuf, "%d", err);
    }
    _error = msgbuf;
    return -1;
}

int HTTPClient::_waitFor(Callback_t callback)
{
    while (millis() < _endTime) {
        int result = callback();
        if (result != 0) {
            return result;
        }
        delay(1);
    }
    _error = "Timeout";
    return -1;
}

int HTTPClient::_request() 
{
    int iResult;

    _httpCode = 0;
    _body.clear();
    _endTime = (_timeout * 1000) + millis();

    IPAddress address;
    if (!WiFi.hostByName(_host.c_str(), address)) {
        _error = "Could not resolve host";
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.S_un.S_addr = (uint32_t)address;

    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_socket == INVALID_SOCKET) {
        _error = "Failed to create socket";
        return -1;
    }

    u_long iMode = 0; // BLOCKING
    iResult = ioctlsocket(_socket, FIONBIO, &iMode);
    if (iResult == SOCKET_ERROR) {
        _close();
        return _getLastError();
    }

    iResult = connect(_socket, (SOCKADDR *)&addr, sizeof(addr));
    if (iResult == SOCKET_ERROR) {
        _close();
        _error = "Failed to connect to host";
        return -1;
    }

    if (!_path.length()) {
        _path += '/';
    }

    addHeader("Content-Length", String(_payload.length()));

    String newline = "\r\n";

    _requestHeaders = _method;
    _requestHeaders.printf(" %s HTTP/1.1", _path.c_str());
    _requestHeaders += newline;
    if (_port != 80) {
        _requestHeaders.printf("Host: %s:%u", _host.c_str(), _port);
    } else {
        _requestHeaders.printf("Host: %s", _host.c_str());
    }
    _requestHeaders += newline;
    for (auto& header : _headersList) {
        _requestHeaders.printf("%s: %s", header.name.c_str(), header.value.c_str());
        _requestHeaders += newline;
    }
    _requestHeaders += "Connection: close";
    _requestHeaders += newline;
    _requestHeaders += newline;

    iResult = send(_socket, _requestHeaders.c_str(), _requestHeaders.length(), 0);
    if (iResult == SOCKET_ERROR) {
        _close();
        return _getLastError();
    }

    if (_payload.length()) {
        iResult = send(_socket, _payload.c_str(), _payload.length(), 0);
        if (iResult == SOCKET_ERROR) {
            _close();
            return _getLastError();
        }
    }

    bool isHeader = true;
    DWORD received = 0;
    DWORD flags = 0;
    WSABUF wsaBuf;
    char buffer[1024];
    wsaBuf.len = sizeof(buffer) - 1;
    wsaBuf.buf = buffer;
    _headers = String();

    do {
        if (millis() > _endTime) {
            _close();
            _error = "Timeout";
            return -1;
        }
        iResult = WSARecv(_socket, &wsaBuf, 1, &received, &flags, NULL, NULL);
        if (iResult == SOCKET_ERROR) {
            _close();
            return _getLastError();
        }
        if (received > 0) {
            buffer[received] = 0;
            if (isHeader) {
                _headers += buffer;
                const char* bodyPtr = nullptr;
                auto endHeader = _requestHeaders.indexOf("\n\n");
                if (endHeader == -1) {
                    endHeader = _headers.indexOf("\r\n\r\n");
                    if (endHeader != -1) {
                        bodyPtr = _headers.c_str() + endHeader + 4;
                    }
                }
                else {
                    bodyPtr = _headers.c_str() + endHeader + 2;
                }
                if (bodyPtr) {
                    isHeader = false;
                    if (bodyPtr != _headers.c_str() + _headers.length()) {
                        _body.write((const uint8_t *)bodyPtr, received - (bodyPtr - _headers.c_str()));
                    }
                    _headers.replace(String('\r'), emptyString);
                    _headers.remove(_headers.indexOf("\n\n"), _headers.length());
                }
            } else {
                _body.write((const uint8_t *)buffer, received);
            }
        }
    } while (received != 0);

    _close();

    if (isHeader) {
        _headers.replace(String('\r'), emptyString);
    }

    if (!_headers.startsWith("HTTP/")) {
        _httpCode = -1; // invalid response
        return _httpCode;
    }

    int pos = _headers.indexOf(' ');
    if (pos == -1) {
        _httpCode = 500;
        return _httpCode;
    }
    _httpCode = _headers.substring(pos + 1).toInt();

    return _httpCode;
}

size_t HTTPClient::getSize() {
    return _body.length();
}

Stream &HTTPClient::getStream() {
    return _body;
}

void HTTPClient::addHeader(const String& name, const String& value, bool first, bool replace)
{
    auto header = HttpHeader(name, value);
    if (replace) {
        _headersList.erase(std::remove(_headersList.begin(), _headersList.end(), header), _headersList.end());
    }
    if (first) {
        _headersList.insert(_headersList.begin(), std::move(header));
    }
    else {
        _headersList.push_back(std::move(header));
    }
}

void HTTPClient::setAuthorization(const char* auth)
{
    if (auth) {
        String _auth = auth;
        _auth.replace(String('\n'), emptyString);
        addHeader("Authorization", _auth);
    }
}

void HTTPClient::dump(Print &output, int maxLen)
{
    output.printf("URL: %s\n", _url.c_str());
    if (_error.length()) {
        output.printf("Error: %s\n", _error.c_str());
    }
    if (_payload.length()) {
        output.println(F("Payload:"));
        output.println(_payload);
    }
    output.printf("Request headers:\n%s", _requestHeaders.c_str());
    output.printf("Http code %d\n", _httpCode);
    output.printf("Headers:\n%s\n", _headers.c_str());
    output.printf("Body (%u):\n%.*s\n", _body.length(), maxLen, _body.c_str());
}

void HTTPClient::_close()
{
    if (_lastError == 0) {
        _lastError = WSAGetLastError();
    }
    if (_socket != INVALID_SOCKET) {
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
}

#endif
