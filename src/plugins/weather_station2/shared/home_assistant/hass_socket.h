/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// One connection to the websocket API of Home Assistant, owned by the request task (Client).
//
// The screen used to poll `POST /api/template` every `hass.poll` seconds: a new TCP connection
// (and with `https` a whole TLS handshake) per poll, a render of the template and a response -
// whether or not a single value changed. This connection replaces the poll:
//
//   * `render_template` subscribes the template the screen already builds. Home Assistant
//     re-renders it when one of the entities it reads changes and sends the result **only when it
//     differs from the previous one** (verified in `homeassistant/helpers/event.py`, identical
//     results are not sent), so a value that does not change costs nothing at all and a change
//     arrives in about 100 ms instead of within the next poll interval.
//   * `call_service` sends the actions of the tiles over the same connection (no extra handshake,
//     no extra TLS session).
//   * `recorder/statistics_during_period` reads the history graph of the sensor panel, in chunks of
//     six hours (see kChunkSeconds) - the receive path of the ESP32 stalls in the middle of a
//     single websocket message larger than about 8 KB.
//
// The screen is only subscribed while it is visible (the client drops the subscription when it is
// left), and the values are refreshed in full every `hass.poll` seconds as a safety net: a
// subscription is not a guarantee (a connection that was dropped, an update of Home Assistant or a
// value that changed while the screen was off cannot be reported), everything else is pushed.
//
// The class is used by the request task only and locks nothing.
//

#include <Arduino_compat.h>
#include <PrintString.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace WeatherStation2 {
namespace HomeAssistant {

class Socket {
public:
    // one bucket of the statistics: epoch seconds of its start and the mean of the 5 minutes
    struct Point {
        uint32_t time{0};
        float mean{0};
    };

    // 5 minute buckets of a 48 hour window plus the bucket that is running at the moment
    static constexpr uint16_t kMaxPoints = (48 * 60) / 5 + 1; // 577

    // Length of one statistics request. A 48 hour window is about 32 KB of JSON and the receive
    // path of the ESP32 hands over only a part of it before it waits for the server to send more,
    // which it does not (verified on the device: the transfer of a 32 KB answer stops after 8256
    // bytes and the timeout fires). Six hours are about 4 KB - one burst, no update of the window
    // needed - and the chunks are requested one after the other on the same connection
    static constexpr uint32_t kChunkSeconds = 6 * 3600;
    // connecting, upgrading and authenticating need a few round trips
    static constexpr uint32_t kConnectTimeout = 20000;
    // a chunk that did not arrive within this deadline is treated as lost
    static constexpr uint32_t kChunkTimeout = 8000;
    // Grace period of a message whose frame header was read already: the task pumps the
    // connection with a short deadline (nothing to read is the normal case), a message that was
    // just started is given this much time to arrive completely - otherwise a frame that is
    // delivered slowly (a TLS record on a slow link) would look like a dead connection, and a
    // half read frame cannot be repaired at all
    static constexpr uint32_t kBodyTimeout = 2000;
    // A connection that stays open but delivers nothing is not usable: the API answers every ping
    // (kPingInterval), so a silence of this length means the peer is gone or the connection was
    // dropped without a FIN (a NAT that forgot it). It is given up and opened again
    static constexpr uint32_t kSilenceTimeout = 70000;
    // deadline of a whole statistics fetch, the nine chunks of a 48 hour window are read in about
    // a second
    static constexpr uint32_t kFetchTimeout = 60000;
    // Largest accepted message: a statistics chunk is about 4 KB and the rendered template of a
    // page a few KB as well (the stall above is the reason to keep both small). A larger message
    // is a protocol error, the connection is dropped and opened again
    static constexpr uint32_t kMaxMessage = 16384;
    // largest rendered template result that is handed over (a message that is no template result
    // must not be able to exhaust the heap)
    static constexpr size_t kMaxResult = 8192;
    // Home Assistant closes an idle websocket (aiohttp pings and expects a pong): send a ping of
    // our own well below that limit, the answer also proves that the connection is alive
    static constexpr uint32_t kPingInterval = 25000;
    // service calls that wait for their answer (the `result` message of the API)
    static constexpr uint8_t kMaxPendingActions = 8;

    Socket();
    ~Socket();

    // Opens the connection: parses the URL of the configuration, connects, upgrades to the
    // websocket protocol and authenticates the session (`url` is the URL of/from /hass.yaml)
    bool open(const char *url, const char *token);
    void close();
    bool isOpen() const {
        return _open;
    }

    // Reads messages for at most `timeout` ms and handles them (events, pings, answers). false
    // when the connection is dead, the reason is in getError()
    bool pump(uint32_t timeout);
    // sends `{"type":"ping"}`, Home Assistant answers with a pong
    bool ping();

    // Subscribes the rendered template (the previous subscription is dropped first, the result of
    // the page that is left is not wanted). `templateText` is the escaped JSON string of the
    // template body and `page` the page it was built from: the result is stamped with it, so a
    // response that arrives for an old subscription is not applied to the page that is shown now
    bool subscribeTemplate(const String &templateText, uint8_t page);
    void unsubscribeTemplate();
    bool isSubscribed() const {
        return _templateId != 0;
    }
    // Sends a service call. `serviceData` is the JSON object with the service data (it carries the
    // entity id as well). The tile is handed back by takeActionFailure() when the call failed
    bool callService(uint8_t tile, const char *domain, const char *service, const char *serviceData);

    // Statistics of an entity over the last `hours` hours, requested in chunks (see kChunkSeconds)
    // over the open connection. `points` has to have room for kMaxPoints buckets. Returns false
    // when the window could not be read completely, the buckets that arrived are kept (and the
    // caller reports the error)
    bool fetchStats(const char *entity, uint8_t hours, Point *points, uint16_t &count, uint32_t &start, uint32_t &end, String &error);

    // rendered template that changed since the last call, or the error its render (or the
    // subscription itself) reported. `page` is the page the template was built from
    bool takeTemplateResult(String &text, String &error, uint8_t &page);
    // one tile whose service call failed
    bool takeActionFailure(uint8_t &tile);

    const char *getError() const {
        return _error.c_str();
    }
    uint32_t getMessageCount() const {
        return _messageCount;
    }

private:
    // one service call that waits for its answer
    struct PendingAction {
        uint16_t id{0};
        uint8_t tile{0};
        bool used{false};
    };

    // connect, upgrade and authenticate
    bool _openConnection();
    void _openConnectionDone();
    // parses the URL of the configuration
    bool _parseUrl(const char *url);
    // `GET /api/websocket` and the headers of the answer
    bool _handshake(uint32_t deadline);
    bool _readHeaders(uint32_t deadline);
    void _readErrorBody(uint32_t deadline);
    bool _readLine(char *line, size_t size, uint32_t deadline);
    // one websocket message, handled by _dispatch() when it is complete
    bool _readMessage(uint32_t deadline);
    // handles the message in _message (events, answers, pings)
    void _dispatch();
    void _handleTemplateEvent();
    void _beginParse();
    // the bucket parser of the statistics (the message in _message)
    void _parseStats(const char *text, size_t length);
    void _append(char chr);
    void _parseObject();
    // value of a member of the message
    const char *_value(const char *key) const;
    uint16_t _id() const;
    bool _truthy(const char *key) const;
    // JSON string value of a member, unescaped
    String _stringValue(const char *key) const;
    // Text of the `result` member of an event: the rendered template. Home Assistant answers it as
    // a string, or - when the render is valid JSON itself, which is what the template of a page
    // produces - as the parsed object. Both are handed over as the text of the JSON they carry
    String _resultText() const;
    // one statistics chunk of the window
    bool _requestChunk(uint32_t start, uint32_t end, const char *entity, String &error);
    // remembers a service call and reports the failed ones
    void _rememberAction(uint16_t id, uint8_t tile);
    bool _failAction(uint16_t id);
    void _failure(uint8_t tile);
    // sends a text frame (the frames of a client are masked)
    bool _sendText(const char *text);
    // sends a control frame
    bool _sendControl(uint8_t opcode, const uint8_t *payload = nullptr, size_t length = 0);
    void _sendPong(uint16_t id);
    // A write that did not go through means the connection is dead: it must not be used any more,
    // the client opens a new one. A broken socket can look alive for a long time (the reads only
    // report it when their timeout runs out), which is exactly what a network that went away in
    // between leaves behind
    void _markDead(const __FlashStringHelper *reason);
    // small helpers of the protocol
    void _stage(const char *name) {
        _stageName = name;
    }
    bool _readExact(void *buffer, size_t length, uint32_t deadline);

private:
    // the socket: plain HTTP or TLS. The panel has no CA store, so an instance behind https is
    // only reachable with setInsecure() - the same thing the REST requests do
    WiFiClient _plain;
    WiFiClientSecure _secure;
    WiFiClient *_socket{nullptr};

    char _host[64]{};
    uint16_t _port{8123};
    bool _secureUrl{false};
    // the URL and the token of the connection, kept for the reconnect of a statistics fetch
    String _url;
    String _token;
    bool _open{false};

    // the message that is read and its length (PSRAM, kMaxMessage + 1 bytes)
    char *_message{nullptr};
    uint32_t _messageLength{0};
    uint32_t _messageCount{0};
    bool _inMessage{false};

    // ids of the API. 0 is never used, it means "none"
    uint16_t _nextId{1};
    uint16_t _templateId{0};
    uint16_t _statsId{0};

    // the page the subscription of _templateId was built from, and the page of the result that
    // waits in _templateResult (0xff means "none")
    static constexpr uint8_t kNoPage = 0xff;
    uint8_t _templatePage{kNoPage};
    uint8_t _resultPage{kNoPage};
    // millis() of the last message that arrived (a pong counts): the silence watchdog of pump()
    uint32_t _lastMessage{0};

    // the rendered template and the error of its render
    String _templateResult;
    String _templateError;
    bool _templateValid{false};

    // statistics of the request that is running
    Point *_points{nullptr};
    uint16_t _count{0};
    uint16_t _maxPoints{0};
    bool _statsAnswered{false};
    bool _statsSuccess{false};
    String _statsMessage;

    // parser state of the buckets
    char _object[256]{};
    uint16_t _objectLength{0};
    uint8_t _objectDepth{0};
    uint8_t _arrayDepth{0};
    bool _inObject{false};
    bool _inString{false};
    bool _escape{false};
    // the object of the bucket does not fit into _object, it is dropped
    bool _overflow{false};

    // service calls that wait for their answer
    PendingAction _actions[kMaxPendingActions];
    // tiles whose service call failed, reported by takeActionFailure(). The value is the tile
    // index + 1, 0 means that the slot is free
    uint8_t _failed[kMaxPendingActions]{};

    // keepalive
    uint32_t _nextPing{0};
    // result of a rejected upgrade
    char _body[192]{};
    // reason of a failure and the phase that is running (part of a timeout message)
    String _error;
    const char *_stageName{"start"};
};

} // namespace HomeAssistant
} // namespace WeatherStation2
