/**
 * Author: sascha_lammers@gmx.de
 */

#include "hass_socket.h"

#include <StrView.h>
#include <stdarg.h>

#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 1
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WeatherStation2 {
namespace HomeAssistant {

// ------------------------------------------------------------------------------------------
// helpers
// ------------------------------------------------------------------------------------------
namespace {

// 2020-09-13: before that the clock was not set (NTP) and the window of the statistics would be
// wrong
constexpr time_t kMinValidTime = 1600000000;
// length of one bucket of the recorder
constexpr uint32_t kBucketSeconds = 300;
// protocol of the API: the statistics are requested in these buckets
constexpr const char *kPeriod = "5minute";

// Sec-WebSocket-Key of the handshake: 16 random bytes as base64, which is 24 characters (the last
// two are the "==" padding of the single byte that is left over). Home Assistant decodes the key
// and rejects the upgrade with "400 Handshake error" when it is not exactly 16 bytes - a key of 18
// bytes (24 characters without padding) is not accepted
void _randomKey(char *output)
{
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t random[16];
    for (uint8_t i = 0; i < 4; i++) {
        const uint32_t value = esp_random();
        memcpy(random + i * sizeof(value), &value, sizeof(value));
    }
    size_t position = 0;
    for (uint8_t i = 0; i < sizeof(random); i += 3) {
        const auto remaining = static_cast<uint8_t>(sizeof(random) - i);
        const uint32_t value = (static_cast<uint32_t>(random[i]) << 16) |
                               ((remaining > 1) ? (static_cast<uint32_t>(random[i + 1]) << 8) : 0) |
                               ((remaining > 2) ? static_cast<uint32_t>(random[i + 2]) : 0);
        output[position++] = kAlphabet[(value >> 18) & 0x3f];
        output[position++] = kAlphabet[(value >> 12) & 0x3f];
        output[position++] = (remaining > 1) ? kAlphabet[(value >> 6) & 0x3f] : '=';
        output[position++] = (remaining > 2) ? kAlphabet[value & 0x3f] : '=';
    }
    output[position] = 0;
}

// "2026-09-30T10:00:00+00:00". Home Assistant validates start_time/end_time with cv.datetime and
// every parser it can use accepts an explicit offset (a "Z" does not need to be supported by all
// of them)
void _formatIso(time_t value, char *output, size_t size)
{
    struct tm tm;
    gmtime_r(&value, &tm);
    strftime(output, size, "%Y-%m-%dT%H:%M:%S+00:00", &tm);
}

// value of one hexadecimal digit
uint32_t _hexValue(char chr)
{
    if (chr >= '0' && chr <= '9') {
        return static_cast<uint32_t>(chr - '0');
    }
    if (chr >= 'a' && chr <= 'f') {
        return static_cast<uint32_t>(chr - 'a' + 10);
    }
    if (chr >= 'A' && chr <= 'F') {
        return static_cast<uint32_t>(chr - 'A' + 10);
    }
    return 0;
}

// appends the UTF-8 encoding of a code point (a JSON `\uXXXX` escape of the rendered template)
void _appendUtf8(String &output, uint32_t code)
{
    if (code < 0x80) {
        output += static_cast<char>(code);
    }
    else if (code < 0x800) {
        output += static_cast<char>(0xc0 | (code >> 6));
        output += static_cast<char>(0x80 | (code & 0x3f));
    }
    else if (code < 0x10000) {
        output += static_cast<char>(0xe0 | (code >> 12));
        output += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        output += static_cast<char>(0x80 | (code & 0x3f));
    }
}

} // namespace

// ------------------------------------------------------------------------------------------
// lifecycle
// ------------------------------------------------------------------------------------------
Socket::Socket()
{
}

Socket::~Socket()
{
    close();
    free(_message);
    _message = nullptr;
}

void Socket::close()
{
    if (_socket) {
        _socket->stop();
        _socket = nullptr;
    }
    _open = false;
    _inMessage = false;
    _messageLength = 0;
    _lastMessage = 0;
    _templateId = 0;
    _templatePage = kNoPage;
    _resultPage = kNoPage;
    _statsId = 0;
    _statsAnswered = true;
    _statsMessage = String();
    _body[0] = 0;
    for (auto &action : _actions) {
        action.used = false;
    }
    free(_message);
    _message = nullptr;
}

bool Socket::open(const char *url, const char *token)
{
    close();
    _error = String();
    if (!token || !token[0]) {
        _error = String("no token");
        return false;
    }
    if (!_parseUrl(url)) {
        return false;
    }
    _url = url;
    _token = token;
    _nextId = 1;
    if (!_message) {
        _message = static_cast<char *>(ps_malloc(kMaxMessage + 1));
        if (!_message) {
            _error = String("out of memory");
            return false;
        }
    }
    if (_secureUrl) {
        _secure.setInsecure();
        _socket = &_secure;
    }
    else {
        _socket = &_plain;
    }
    _message[0] = 0;
    if (!_openConnection()) {
        _socket->stop();
        _socket = nullptr;
        free(_message);
        _message = nullptr;
        return false;
    }
    _nextPing = millis() + kPingInterval;
    return true;
}

void Socket::_openConnectionDone()
{
    _open = true;
    _inMessage = false;
    _messageLength = 0;
    _lastMessage = millis();
}

bool Socket::_openConnection()
{
    const auto deadline = millis() + kConnectTimeout;
    _stage("connecting");
    if (!_socket->connect(_host, _port)) {
        _error = String();
        StrWrapper(_error).printf("cannot connect to %s:%u", _host, static_cast<unsigned>(_port));
        return false;
    }
    _stage("handshake");
    if (!_handshake(deadline)) {
        return false;
    }
    // the first message of the API asks for the authentication
    _stage("auth_required");
    if (!_readMessage(deadline)) {
        return false;
    }
    if (!strstr(_message, "\"auth_required\"")) {
        _error = "the server does not request a token: ";
        _error += _message;
        return false;
    }
    String auth("{\"type\":\"auth\",\"access_token\":\"");
    auth += _token;
    auth += "\"}";
    _stage("auth");
    if (!_sendText(auth.c_str())) {
        return false;
    }
    _stage("auth_ok");
    if (!_readMessage(deadline)) {
        return false;
    }
    if (strstr(_message, "\"auth_invalid\"")) {
        _error = "Home Assistant rejected the token: ";
        _error += _stringValue("message");
        return false;
    }
    if (!strstr(_message, "\"auth_ok\"")) {
        _error = "no answer to the authentication: ";
        _error += _message;
        return false;
    }
    _openConnectionDone();
    return true;
}

bool Socket::_parseUrl(const char *url)
{
    _host[0] = 0;
    _port = 8123;
    _secureUrl = false;
    if (!url || !url[0]) {
        _error = String("no URL");
        return false;
    }
    // the URL of the configuration, ws:// and wss:// are accepted as well (the API is always at
    // /api/websocket, a path of the URL is ignored)
    const char *start = url;
    if (!strncasecmp(start, "https://", 8) || !strncasecmp(start, "wss://", 6)) {
        _secureUrl = true;
        _port = 443;
        start += (start[4] == ':') ? 6 : 8;
    }
    else if (!strncasecmp(start, "http://", 7) || !strncasecmp(start, "ws://", 5)) {
        start += (start[2] == ':') ? 5 : 7;
    }
    size_t length = 0;
    while (start[length] && start[length] != ':' && start[length] != '/' && length + 1 < sizeof(_host)) {
        _host[length] = start[length];
        length++;
    }
    _host[length] = 0;
    if (!length) {
        _error = String("invalid URL");
        return false;
    }
    if (start[length] == ':') {
        const auto port = atoi(start + length + 1);
        if (port < 1 || port > 65535) {
            _error = String("invalid port");
            return false;
        }
        _port = static_cast<uint16_t>(port);
    }
    return true;
}

// ------------------------------------------------------------------------------------------
// handshake
// ------------------------------------------------------------------------------------------
bool Socket::_handshake(uint32_t deadline)
{
    char key[32];
    _randomKey(key);
    String request;
    StrWrapper(request).printf("GET /api/websocket HTTP/1.1\r\nHost: %s:%u\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",
                               _host, static_cast<unsigned>(_port), key);
    if (_socket->write(reinterpret_cast<const uint8_t *>(request.c_str()), request.length()) != request.length()) {
        _error = String("cannot send the handshake");
        return false;
    }
    return _readHeaders(deadline);
}

bool Socket::_readHeaders(uint32_t deadline)
{
    char status[160];
    // "HTTP/1.1 101 Switching Protocols"
    if (!_readLine(status, sizeof(status), deadline)) {
        return false;
    }
    const bool switching = strstr(status, " 101") != nullptr;
    // the headers are skipped either way, the first empty line ends them
    char line[160];
    for (uint8_t i = 0; i < 32; i++) {
        if (!_readLine(line, sizeof(line), deadline)) {
            return false;
        }
        if (!line[0]) {
            break;
        }
    }
    if (switching) {
        return true;
    }
    StrWrapper(status).replace('\r', ' ');
    // the reason of a rejected upgrade is in the body (Home Assistant answers a short text, for
    // example "Handshake error: '<key>'")
    _readErrorBody(millis() + 500);
    _error = "the server did not switch protocols (";
    _error += status;
    if (_body[0]) {
        _error += "): ";
        _error += _body;
    }
    else {
        _error += ")";
    }
    return false;
}

// Whatever the server sent after the headers. A rejected upgrade is answered with a short text
// ("Missing Sec-WebSocket-Key"), the body is only read to report the reason - the connection is
// dropped right after
void Socket::_readErrorBody(uint32_t deadline)
{
    _body[0] = 0;
    size_t length = 0;
    while (length + 1 < sizeof(_body)) {
        if (_socket->available() <= 0) {
            if (static_cast<int32_t>(millis() - deadline) >= 0 || !_socket->connected()) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        const auto count = _socket->read(reinterpret_cast<uint8_t *>(_body) + length, sizeof(_body) - 1 - length);
        if (count <= 0) {
            break;
        }
        length += static_cast<size_t>(count);
    }
    _body[length] = 0;
    // the body may be JSON or plain text with new lines, they would break the one line trace
    for (size_t i = 0; i < length; i++) {
        if (_body[i] == '\r' || _body[i] == '\n') {
            _body[i] = ' ';
        }
    }
}

bool Socket::_readLine(char *line, size_t size, uint32_t deadline)
{
    size_t length = 0;
    for (;;) {
        uint8_t chr = 0;
        if (!_readExact(&chr, 1, deadline)) {
            return false;
        }
        if (chr == '\n') {
            break;
        }
        if (length + 1 < size) {
            line[length++] = static_cast<char>(chr);
        }
        else {
            // the line is longer than the buffer, the rest is dropped (the content of a header
            // does not matter)
            length = size - 1;
        }
    }
    // "\r\n" of the line end
    if (length && line[length - 1] == '\r') {
        length--;
    }
    line[length] = 0;
    return true;
}

// ------------------------------------------------------------------------------------------
// messages
// ------------------------------------------------------------------------------------------
bool Socket::_readMessage(uint32_t deadline)
{
    for (;;) {
        uint8_t header[2];
        if (!_readExact(header, sizeof(header), deadline)) {
            return false;
        }
        const bool finished = (header[0] & 0x80) != 0;
        const auto opcode = static_cast<uint8_t>(header[0] & 0x0f);
        const bool masked = (header[1] & 0x80) != 0;
        uint32_t length = header[1] & 0x7f;
        if (length == 126) {
            uint8_t extended[2];
            if (!_readExact(extended, sizeof(extended), deadline)) {
                return false;
            }
            length = (static_cast<uint32_t>(extended[0]) << 8) | extended[1];
        }
        else if (length == 127) {
            uint8_t extended[8];
            if (!_readExact(extended, sizeof(extended), deadline)) {
                return false;
            }
            // a message of a few hundred megabytes is not expected
            for (uint8_t i = 0; i < 4; i++) {
                if (extended[i]) {
                    _error = String("message too large");
                    _open = false;
                    return false;
                }
            }
            length = (static_cast<uint32_t>(extended[4]) << 24) | (static_cast<uint32_t>(extended[5]) << 16) |
                     (static_cast<uint32_t>(extended[6]) << 8) | extended[7];
        }
        if (length > kMaxMessage) {
            _error = String("message too large");
            _open = false;
            return false;
        }
        // a frame was started: the rest of it is given the body grace period instead of the short
        // deadline of the pump
        const uint32_t bodyDeadline = (static_cast<int32_t>(millis() + kBodyTimeout - deadline) > 0) ? millis() + kBodyTimeout : deadline;
        uint8_t mask[4] = { 0, 0, 0, 0 };
        if (masked && !_readExact(mask, sizeof(mask), bodyDeadline)) {
            return false;
        }
        // the frames of the server are not masked (RFC 6455), a masked one is handled anyway
        if (opcode == 0x8) {
            _error = String("the server closed the connection");
            _open = false;
            return false;
        }
        if (opcode == 0x9 && length > 125) {
            _error = String("invalid ping");
            _open = false;
            return false;
        }
        // a message of the API starts with opcode 1, the frames of a fragmented one continue with
        // opcode 0
        if (opcode == 0x1 || opcode == 0x2) {
            _inMessage = true;
            _messageLength = 0;
            _beginParse();
        }

        uint8_t buffer[192];
        uint32_t offset = 0;
        while (length) {
            const auto chunk = static_cast<size_t>((length > sizeof(buffer)) ? sizeof(buffer) : length);
            if (!_readExact(buffer, chunk, bodyDeadline)) {
                return false;
            }
            if (masked) {
                for (size_t i = 0; i < chunk; i++) {
                    buffer[i] ^= mask[(offset + i) & 3];
                }
            }
            if ((opcode == 0x1 || opcode == 0x0) && _inMessage) {
                if (_messageLength + chunk > kMaxMessage) {
                    _error = String("message too large");
                    _open = false;
                    return false;
                }
                memcpy(_message + _messageLength, buffer, chunk);
                _messageLength += static_cast<uint32_t>(chunk);
                _message[_messageLength] = 0;
            }
            else if (opcode == 0x9 && !_sendControl(0xa, buffer, chunk)) {
                return false;
            }
            offset += static_cast<uint32_t>(chunk);
            length -= static_cast<uint32_t>(chunk);
        }
        // a control frame is a message of its own and does not end the one that is read
        if ((opcode == 0x9 || opcode == 0xa) && finished) {
            continue;
        }
        if (finished) {
            if (opcode == 0x2) {
                // a binary message carries no data of the API
                _inMessage = false;
                continue;
            }
            _inMessage = false;
            _messageCount++;
            _dispatch();
            return true;
        }
    }
}

// Handles a message: the rendered template of the subscription, the answer of a statistics chunk
// or of a service call, and the ping of the API
void Socket::_dispatch()
{
    const auto type = _value("type");
    if (!type || *type != '"') {
        return;
    }
    // The live trace of the protocol: what arrives with which id (the pings of the API and our own
    // pongs are left out, they arrive every kPingInterval and carry nothing). A subscription that
    // does not deliver is told apart from one whose events are dropped by this line
    if (strncmp(type, "\"ping\"", 6) && strncmp(type, "\"pong\"", 6)) {
        char tag[12]{};
        for (size_t i = 0; i + 1 < sizeof(tag) && type[1 + i] && type[1 + i] != '"'; i++) {
            tag[i] = type[1 + i];
        }
        __LDBG_printf("hass> ws< %s id=%u (%u bytes, template %u)", tag, static_cast<unsigned>(_id()),
                      static_cast<unsigned>(_messageLength), static_cast<unsigned>(_templateId));
    }
    if (!strncmp(type, "\"event\"", 7)) {
        if (_templateId && _id() == _templateId) {
            _handleTemplateEvent();
        }
        return;
    }
    if (!strncmp(type, "\"result\"", 8)) {
        const auto id = _id();
        if (_statsId && id == _statsId) {
            _statsSuccess = _truthy("success");
            if (_statsSuccess) {
                _beginParse();
                _parseStats(_message, _messageLength);
            }
            else {
                _statsMessage = _stringValue("message");
            }
            _statsAnswered = true;
            return;
        }
        if (id && !_truthy("success")) {
            // A service call that failed - or a subscription that could not be created. A rejected
            // subscription would leave the client waiting for events that never come: it is
            // reported like an error of the render and the subscription is given up, so the client
            // subscribes again (a template that the API does not accept would repeat, a page that
            // could not be subscribed is retried the same way the next resync does)
            if (id == _templateId) {
                const auto message = _stringValue("message");
                const auto code = _stringValue("code");
                _templateId = 0;
                _templatePage = kNoPage;
                _templateResult = String();
                if (code.length()) {
                    _templateError = code;
                    _templateError += ": ";
                    _templateError += message;
                }
                else {
                    _templateError = message;
                }
                _resultPage = kNoPage;
                _templateValid = true;
            }
            else {
                _failAction(id);
            }
        }
        return;
    }
    if (!strncmp(type, "\"ping\"", 6)) {
        // the JSON ping of the API (a plain websocket ping frame is answered by _readMessage)
        _sendPong(_id());
        return;
    }
    // "pong" and the answer of a subscription are not of interest
}

void Socket::_handleTemplateEvent()
{
    const auto error = _stringValue("error");
    if (error.length()) {
        _templateResult = String();
        _templateError = error;
    }
    else {
        _templateResult = _resultText();
        _templateError = String();
    }
    _resultPage = _templatePage;
    _templateValid = true;
}

bool Socket::takeTemplateResult(String &text, String &error, PageIndex &page)
{
    if (!_templateValid) {
        return false;
    }
    _templateValid = false;
    text = _templateResult;
    error = _templateError;
    page = _resultPage;
    _templateResult = String();
    _templateError = String();
    _resultPage = kNoPage;
    return true;
}

const char *Socket::_value(const char *key) const
{
    if (!_message || !_messageLength) {
        return nullptr;
    }
    // The member ("key":) is matched in place: the key can have any length, the caller does not
    // have to know a limit and no needle buffer is needed. keyLength + 3 bytes are needed from the
    // quote, a match that does not fit into the rest of the message cannot exist
    const auto keyLength = strlen(key);
    const auto messageEnd = _message + _messageLength;
    for (auto ptr = strchr(_message, '"'); ptr; ptr = strchr(ptr + 1, '"')) {
        if (static_cast<size_t>(messageEnd - ptr) < (keyLength + 3)) {
            break;
        }
        if (ptr[keyLength + 1] == '"' && ptr[keyLength + 2] == ':' && !strncmp(ptr + 1, key, keyLength)) {
            auto value = ptr + keyLength + 3;
            while (*value == ' ') {
                value++;
            }
            return value;
        }
    }
    return nullptr;
}

uint16_t Socket::_id() const
{
    const auto ptr = _value("id");
    return ptr ? static_cast<uint16_t>(strtoul(ptr, nullptr, 10)) : 0;
}

// The render of the template. Home Assistant answers the text of the render as a JSON string - or,
// when that text is valid JSON by itself, as the value it parsed out of it. The template of a page
// is a JSON object and every value it writes is one (a state, a number, a tojson of a list), so its
// render is valid JSON and arrives as an object: both forms are handed to the dashboard as the text
// of the JSON it walks (see Dashboard::_applyResponse())
String Socket::_resultText() const
{
    const auto ptr = _value("result");
    if (!ptr || !*ptr) {
        return String();
    }
    if (*ptr == '"') {
        // the render of a template that is not valid JSON (a value that renders empty, an error)
        return _stringValue("result");
    }
    // The raw value of the member. Its nesting is followed and the characters of a string are
    // skipped, so a comma or a brace inside a value does not end it
    const auto limit = static_cast<size_t>(kMaxResult);
    size_t depth = 0;
    size_t length = 0;
    bool inString = false;
    bool escaped = false;
    while (ptr[length] && length < limit) {
        const char chr = ptr[length];
        if (inString) {
            if (escaped) {
                escaped = false;
            }
            else if (chr == '\\') {
                escaped = true;
            }
            else if (chr == '"') {
                inString = false;
            }
            length++;
            continue;
        }
        if (chr == '"') {
            inString = true;
        }
        else if (chr == '{' || chr == '[') {
            depth++;
        }
        else if (chr == '}' || chr == ']') {
            if (!depth) {
                break;
            }
            depth--;
            length++;
            if (!depth) {
                // the object or the array is complete
                break;
            }
            continue;
        }
        else if (!depth) {
            // a scalar (null, a literal, a number): there is no member in it to read
            break;
        }
        length++;
    }
    String value;
    value.reserve(length + 1);
    for (size_t i = 0; i < length; i++) {
        value += ptr[i];
    }
    return value;
}

bool Socket::_truthy(const char *key) const
{
    const auto ptr = _value(key);
    return ptr && !strncmp(ptr, "true", 4);
}

String Socket::_stringValue(const char *key) const
{
    String value;
    const auto ptr = _value(key);
    if (!ptr || *ptr != '"') {
        return value;
    }
    const char *input = ptr + 1;
    value.reserve(64);
    while (*input && *input != '"' && value.length() < kMaxResult) {
        if (*input != '\\') {
            value += *input++;
            continue;
        }
        input++;
        switch (*input) {
        case 'n':
            value += '\n';
            input++;
            break;
        case 'r':
            value += '\r';
            input++;
            break;
        case 't':
            value += '\t';
            input++;
            break;
        case 'b':
            value += '\b';
            input++;
            break;
        case 'f':
            value += '\f';
            input++;
            break;
        case 'u': {
            uint32_t code = 0;
            input++;
            for (uint8_t i = 0; i < 4 && isxdigit(static_cast<unsigned char>(*input)); i++) {
                code = (code << 4) | _hexValue(*input);
                input++;
            }
            _appendUtf8(value, code);
            break;
        }
        default:
            if (*input) {
                value += *input++;
            }
            break;
        }
    }
    return value;
}

// ------------------------------------------------------------------------------------------
// subscription and service calls
// ------------------------------------------------------------------------------------------
bool Socket::subscribeTemplate(const String &templateText, PageIndex page)
{
    if (!_open) {
        _error = String("not connected");
        return false;
    }
    if (!templateText.length()) {
        return false;
    }
    // the subscription of the page that is left is dropped first, its result is not wanted
    unsubscribeTemplate();
    const auto id = _nextId++;
    String command;
    StrWrapper(command).printf("{\"id\":%u,\"type\":\"render_template\",\"strict\":false,\"report_errors\":true,\"template\":\"",
                               static_cast<unsigned>(id));
    command += templateText;
    command += "\"}";
    if (!_sendText(command.c_str())) {
        return false;
    }
    _templateId = id;
    _templatePage = page;
    _templateValid = false;
    _templateResult = String();
    _templateError = String();
    return true;
}

void Socket::unsubscribeTemplate()
{
    if (!_templateId) {
        return;
    }
    const auto subscription = _templateId;
    _templateId = 0;
    _templatePage = kNoPage;
    if (!_open) {
        return;
    }
    // 61 bytes + NUL: unsubscribe_events with two ids of five digits (max uint16_t), no
    // String is allocated for the command
    char command[64];
    snprintf(command, sizeof(command), "{\"id\":%u,\"type\":\"unsubscribe_events\",\"subscription\":%u}",
             static_cast<unsigned>(_nextId++), static_cast<unsigned>(subscription));
    _sendText(command);
}

bool Socket::callService(TileIndex tile, const char *domain, const char *service, const char *serviceData)
{
    if (!_open) {
        _error = String("not connected");
        _failure(tile);
        return false;
    }
    const auto id = _nextId++;
    String command;
    StrWrapper(command).printf("{\"id\":%u,\"type\":\"call_service\",\"domain\":\"%s\",\"service\":\"%s\",\"service_data\":",
                               static_cast<unsigned>(id), domain, service);
    command += serviceData;
    command += '}';
    if (!_sendText(command.c_str())) {
        _failure(tile);
        return false;
    }
    _rememberAction(id, tile);
    return true;
}

void Socket::_rememberAction(uint16_t id, TileIndex tile)
{
    for (auto &action : _actions) {
        if (!action.used) {
            action.id = id;
            action.tile = tile;
            action.used = true;
            return;
        }
    }
}

// the answer of a service call that failed. The tile is reported to the dashboard, which reverts
// the optimistic state of the screen
bool Socket::_failAction(uint16_t id)
{
    for (auto &action : _actions) {
        if (action.used && action.id == id) {
            action.used = false;
            _failure(action.tile);
            return true;
        }
    }
    return false;
}

void Socket::_failure(TileIndex tile)
{
    for (auto &undefined : _failed) {
        if (!undefined) {
            undefined = tile + 1;
            return;
        }
    }
}

bool Socket::takeActionFailure(TileIndex &tile)
{
    for (auto &undefined : _failed) {
        if (undefined) {
            tile = undefined - 1;
            undefined = 0;
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------------------------------
// history graph of a sensor panel
// ------------------------------------------------------------------------------------------
bool Socket::fetchStats(const char *entity, uint8_t hours, Point *points, uint16_t &count, uint32_t &start, uint32_t &end, String &error)
{
    count = 0;
    error = String();
    if (!_open) {
        error = String("not connected");
        return false;
    }
    if (!entity || !entity[0]) {
        error = String("no entity");
        return false;
    }
    if (hours < 1 || hours > 48) {
        hours = 24;
    }
    const auto now = time(nullptr);
    if (now < kMinValidTime) {
        // the window of the request is built from the clock of the device
        error = String("the clock is not set");
        return false;
    }
    // The window ends now and starts `hours` before it, rounded down to a bucket boundary: the
    // buckets of the answer are aligned to the grid of the graph that way (the bucket that is
    // running at the moment is included)
    _points = points;
    _maxPoints = kMaxPoints;
    _count = 0;
    end = static_cast<uint32_t>(now);
    start = static_cast<uint32_t>((now - static_cast<time_t>(hours) * 3600) / kBucketSeconds * kBucketSeconds);

    bool ok = true;
    bool reconnected = false;
    uint32_t chunkStart = start;
    const auto fetchDeadline = millis() + kFetchTimeout;
    while (chunkStart < end) {
        if (static_cast<int32_t>(millis() - fetchDeadline) >= 0) {
            error = String("the request took too long");
            ok = false;
            break;
        }
        const auto chunkEnd = ((end - chunkStart) > kChunkSeconds) ? (chunkStart + kChunkSeconds) : end;
        if (_requestChunk(chunkStart, chunkEnd, entity, error)) {
            chunkStart = chunkEnd;
            continue;
        }
        // A chunk that is not answered in time leaves the connection in an undefined state. The
        // chunk is requested again on a fresh connection once, the buckets that arrived are kept
        if (reconnected) {
            ok = false;
            break;
        }
        reconnected = true;
        const auto url = _url;
        const auto token = _token;
        __LDBG_printf("hass> statistics chunk %u bytes failed (%s), reconnecting", static_cast<unsigned>(_count), error.c_str());
        close();
        if (!open(url.c_str(), token.c_str())) {
            error = _error;
            ok = false;
            break;
        }
        error = String();
        if (!_requestChunk(chunkStart, chunkEnd, entity, error)) {
            ok = false;
            break;
        }
        chunkStart = chunkEnd;
    }
    count = _count;
    _points = nullptr;
    if (!ok && !error.length()) {
        error = String("the statistics request failed");
    }
    return ok;
}

bool Socket::_requestChunk(uint32_t start, uint32_t end, const char *entity, String &error)
{
    char startTime[32];
    char endTime[32];
    _formatIso(static_cast<time_t>(start), startTime, sizeof(startTime));
    _formatIso(static_cast<time_t>(end), endTime, sizeof(endTime));
    const auto id = _nextId++;
    String request;
    StrWrapper(request).printf("{\"id\":%u,\"type\":\"recorder/statistics_during_period\",\"start_time\":\"%s\",\"end_time\":\"%s\","
                               "\"statistic_ids\":[\"%s\"],\"period\":\"%s\",\"types\":[\"mean\"]}",
                               static_cast<unsigned>(id), startTime, endTime, entity, kPeriod);
    if (!_sendText(request.c_str())) {
        error = _error;
        return false;
    }
    _statsId = id;
    _statsAnswered = false;
    _statsSuccess = false;
    _statsMessage = String();
    const auto deadline = millis() + kChunkTimeout;
    while (!_statsAnswered) {
        if (!_readMessage(deadline)) {
            if (!_open) {
                error = _error.length() ? _error : String("the connection was closed");
                _statsId = 0;
                return false;
            }
            if (static_cast<int32_t>(millis() - deadline) >= 0) {
                error = "timeout while the statistics were read";
                _statsId = 0;
                return false;
            }
        }
    }
    _statsId = 0;
    if (!_statsSuccess) {
        if (_statsMessage.length()) {
            error = "statistics: ";
            error += _statsMessage;
        }
        else {
            error = "the statistics request failed";
        }
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------
// the bucket parser of the statistics
// ------------------------------------------------------------------------------------------
void Socket::_beginParse()
{
    _objectLength = 0;
    _objectDepth = 0;
    _arrayDepth = 0;
    _inObject = false;
    _inString = false;
    _escape = false;
    _overflow = false;
}

void Socket::_parseStats(const char *text, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        const auto chr = text[i];
        if (_inString) {
            // a brace inside a string does not change the nesting
            if (_escape) {
                _escape = false;
            }
            else if (chr == '\\') {
                _escape = true;
            }
            else if (chr == '"') {
                _inString = false;
            }
            if (_inObject) {
                _append(chr);
            }
            continue;
        }
        switch (chr) {
        case '"':
            _inString = true;
            if (_inObject) {
                _append(chr);
            }
            break;
        case '{':
            // the objects of the buckets are the members of the first array of the message (the
            // "result" object around it is not one of them)
            if (_arrayDepth == 1 && !_inObject) {
                _inObject = true;
                _objectDepth = 1;
                _objectLength = 0;
                _overflow = false;
            }
            else if (_inObject) {
                _objectDepth++;
                _append(chr);
            }
            break;
        case '}':
            if (_inObject) {
                _append(chr);
                if (--_objectDepth == 0) {
                    _parseObject();
                    _inObject = false;
                }
            }
            break;
        case '[':
            if (_arrayDepth < 0xff) {
                _arrayDepth++;
            }
            if (_inObject) {
                _append(chr);
            }
            break;
        case ']':
            if (_arrayDepth) {
                _arrayDepth--;
            }
            if (_inObject) {
                _append(chr);
            }
            break;
        default:
            if (_inObject) {
                _append(chr);
            }
            break;
        }
    }
}

void Socket::_append(char chr)
{
    if (_overflow) {
        return;
    }
    if (_objectLength + 1 >= sizeof(_object)) {
        // the object is larger than the buffer: the bucket is dropped, the parser continues with
        // the next one
        _overflow = true;
        return;
    }
    _object[_objectLength++] = chr;
}

// one bucket of the answer:
//   {"start":1759171200000.0,"end":1759171500000.0,"mean":24.53}
// `start` is epoch milliseconds (a float), `mean` is a number or a string, or null while the
// window has no data
void Socket::_parseObject()
{
    if (_overflow || _objectLength < 16 || !_points) {
        return;
    }
    _object[_objectLength] = 0;
    const auto start = strstr(_object, "\"start\":");
    const auto mean = strstr(_object, "\"mean\":");
    if (!start || !mean) {
        return;
    }
    // "start" is epoch **milliseconds**, which does not fit into an unsigned long on a 32 bit
    // target (strtoul would return ULONG_MAX and the bucket would land outside the window)
    const auto milliseconds = strtoull(start + 8, nullptr, 10);
    const auto time = static_cast<uint32_t>(milliseconds / 1000);
    auto text = mean + 7;
    while (*text == ' ') {
        text++;
    }
    float value = 0;
    if (*text == '"') {
        value = strtof(text + 1, nullptr);
    }
    else if (!strncmp(text, "null", 4)) {
        return;
    }
    else {
        value = strtof(text, nullptr);
    }
    if (!time || _count >= _maxPoints) {
        return;
    }
    _points[_count].time = static_cast<uint32_t>(time);
    _points[_count].mean = value;
    _count++;
}

// ------------------------------------------------------------------------------------------
// frames of the protocol
// ------------------------------------------------------------------------------------------
bool Socket::pump(uint32_t timeout)
{
    if (!_open || !_socket) {
        return false;
    }
    // A connection that answers nothing at all is not usable. The API answers every ping, so the
    // watchdog only fires when the peer is gone or the connection was dropped silently - a socket
    // like that looks alive for hours and the values it should push never arrive
    if (_lastMessage && static_cast<uint32_t>(millis() - _lastMessage) >= kSilenceTimeout) {
        _markDead("no answer from the server");
        return false;
    }
    if (static_cast<int32_t>(millis() - _nextPing) >= 0) {
        ping();
    }
    const auto deadline = millis() + timeout;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
        if (!_readMessage(deadline)) {
            // the deadline passed (that is the normal case) or the connection is dead
            return _open;
        }
        _lastMessage = millis();
    }
    return true;
}

bool Socket::ping()
{
    if (!_open) {
        return false;
    }
    _nextPing = millis() + kPingInterval;
    // 26 bytes + NUL: {"id":65535,"type":"ping"}
    char command[32];
    snprintf(command, sizeof(command), "{\"id\":%u,\"type\":\"ping\"}", static_cast<unsigned>(_nextId++));
    return _sendText(command);
}

void Socket::_sendPong(uint16_t id)
{
    // 26 bytes + NUL: {"id":65535,"type":"pong"}
    char command[32];
    snprintf(command, sizeof(command), "{\"id\":%u,\"type\":\"pong\"}", static_cast<unsigned>(id));
    _sendText(command);
}

bool Socket::_sendControl(uint8_t opcode, const uint8_t *payload, size_t length)
{
    if (length > 125) {
        _error = "control frame too large";
        return false;
    }
    uint8_t mask[4];
    const auto value = esp_random();
    memcpy(mask, &value, sizeof(mask));

    uint8_t frame[6 + 125];
    frame[0] = static_cast<uint8_t>(0x80 | opcode);
    frame[1] = static_cast<uint8_t>(0x80 | length);
    memcpy(frame + 2, mask, sizeof(mask));
    for (size_t i = 0; i < length; i++) {
        frame[6 + i] = static_cast<uint8_t>(payload[i] ^ mask[i & 3]);
    }
    const auto size = static_cast<size_t>(6 + length);
    if (_socket->write(frame, size) != size) {
        _markDead("cannot send the frame");
        return false;
    }
    return true;
}

void Socket::_markDead(const char *reason)
{
    if (!_error.length()) {
        _error = String(reason);
    }
    _open = false;
    _templateId = 0;
    _templatePage = kNoPage;
    if (_socket) {
        _socket->stop();
    }
}

bool Socket::_sendText(const char *text)
{
    const auto length = strlen(text);
    uint8_t header[14];
    size_t headerLength = 0;
    header[headerLength++] = 0x81; // FIN + text
    if (length < 126) {
        header[headerLength++] = static_cast<uint8_t>(0x80 | length);
    }
    else if (length <= 0xffff) {
        header[headerLength++] = 0x80 | 126;
        header[headerLength++] = static_cast<uint8_t>(length >> 8);
        header[headerLength++] = static_cast<uint8_t>(length & 0xff);
    }
    else {
        // a request of this client is a few kilobytes at most (the template of a page)
        _error = "the request is too large";
        return false;
    }
    uint8_t mask[4];
    const auto value = esp_random();
    memcpy(mask, &value, sizeof(mask));
    memcpy(header + headerLength, mask, sizeof(mask));
    headerLength += sizeof(mask);

    if (_socket->write(header, headerLength) != headerLength) {
        _markDead("cannot send the request");
        return false;
    }
    // the payload is masked in chunks: a buffer of the full length would be a copy of the request
    // on the stack of the request task (10 KB, which the TLS handshake uses as well)
    uint8_t buffer[64];
    for (size_t offset = 0; offset < length;) {
        const auto chunk = ((length - offset) < sizeof(buffer)) ? (length - offset) : sizeof(buffer);
        for (size_t i = 0; i < chunk; i++) {
            buffer[i] = static_cast<uint8_t>(text[offset + i] ^ mask[(offset + i) & 3]);
        }
        if (_socket->write(buffer, chunk) != chunk) {
            _markDead("cannot send the request");
            return false;
        }
        offset += chunk;
    }
    return true;
}

// ------------------------------------------------------------------------------------------
// reading
// ------------------------------------------------------------------------------------------
// Reads exactly `length` bytes within the deadline.
//
// The bytes are taken one at a time (the pattern HTTPClient/Stream::readBytes uses): the receive
// buffer of the ESP32 has a window of 5760 bytes and a mailbox of six segments, and a read that
// only asks for the bytes `available()` reports stalls in the middle of a message of a few dozen
// kilobytes - the stack then neither hands the rest over nor reopens the window, and the server
// waits for it (verified on the device with a 48 hour answer, which is why the statistics are
// requested in chunks, see kChunkSeconds). `read()` refills the buffer from the socket whenever
// it runs empty, which keeps the window moving.
//
// A timeout reports how far the piece that stalled got, so the trace tells whether a frame header
// or the payload of a message was lost
bool Socket::_readExact(void *buffer, size_t length, uint32_t deadline)
{
    auto *output = static_cast<uint8_t *>(buffer);
    size_t received = 0;
    while (received < length) {
        const auto value = _socket->read();
        if (value >= 0) {
            output[received++] = static_cast<uint8_t>(value);
            continue;
        }
        if (!_socket->connected()) {
            if (!_error.length()) {
                _error = "the connection was closed";
            }
            _open = false;
            return false;
        }
        if (static_cast<int32_t>(millis() - deadline) >= 0) {
            // A timeout before the first byte is the normal case - the task pumps the connection
            // with a short deadline and nothing arrived. A timeout in the middle of a frame or of
            // the payload of a message cannot be repaired: the stream would be read from a wrong
            // offset, so the connection is reported as dead and opened again
            if (received) {
                _error = String();
                StrWrapper(_error).printf("timeout while %s (%u of %u bytes read, %d available)", _stageName,
                                          static_cast<unsigned>(received), static_cast<unsigned>(length), _socket->available());
                _open = false;
                _templateId = 0;
                _templatePage = kNoPage;
            }
            return false;
        }
        // the request task runs on the same core as the idle task and the watchdog, it must not
        // spin in a loop without giving them time
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

} // namespace HomeAssistant
} // namespace WeatherStation2
