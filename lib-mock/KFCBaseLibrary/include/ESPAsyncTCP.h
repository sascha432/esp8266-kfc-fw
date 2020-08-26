
#pragma once

#include <Arduino_compat.h>
#include <functional>

class AsyncClient {
public:
    using err_t = int;
    typedef std::function<void(void *, AsyncClient *)> AcConnectHandler;
    typedef std::function<void(void *, AsyncClient *, size_t len, uint32_t time)> AcAckHandler;
    typedef std::function<void(void *, AsyncClient *, err_t error)> AcErrorHandler;
    typedef std::function<void(void *, AsyncClient *, void *data, size_t len)> AcDataHandler;
    typedef std::function<void(void *, AsyncClient *, struct pbuf *pb)> AcPacketHandler;
    typedef std::function<void(void *, AsyncClient *, uint32_t time)> AcTimeoutHandler;
    typedef std::function<void(void *, size_t event)> AsNotifyHandler;

public:
#if ASYNC_TCP_SSL_ENABLED
    bool connect(const IPAddress &ip, uint16_t port, bool secure = false) {}
    bool connect(const char *host, uint16_t port, bool secure = false) {}
#else
    bool connect(const IPAddress &ip, uint16_t port) { return false; }
    bool connect(const char *host, uint16_t port) { return false; }
#endif
    void close(bool now = false) {}
    void stop() {}
    void abort() {}
    //bool free() { return false;  }
    bool canSend() { return false; }
    size_t space() { return 0; }
    size_t add(const char *data, size_t size, uint8_t apiflags = 0) { return 0; }
    bool send() { return false; }
    size_t ack(size_t len) { return 0; }
    void ackLater() {}
    bool isRecvPush() { return false; }
    size_t write(const char *data) { return 0; }
    size_t write(const char *data, size_t size, uint8_t apiflags = 0) { return 0; }
    uint8_t state() { return 0; }
    bool connecting() { return false; }
    bool connected() { return false; }
    bool disconnecting() { return false; }
    bool disconnected() { return false; }
    bool freeable() { return false; }
    uint16_t getMss() { return false; }
    uint32_t getRxTimeout() { return false; }
    void setRxTimeout(uint32_t timeout) {}
    uint32_t getAckTimeout() { return false; }
    void setAckTimeout(uint32_t timeout) {}
    void setNoDelay(bool nodelay) {}
    bool getNoDelay() { return false; }
    uint32_t getRemoteAddress() { return 0; }
    uint16_t getRemotePort() { return 0; }
    uint32_t getLocalAddress() { return 0; }
    uint16_t getLocalPort() { return 0; }
    IPAddress remoteIP() { return 0; }
    uint16_t  remotePort() { return 0; }
    IPAddress localIP() { return 0; }
    uint16_t  localPort() { return 0; }
    void onConnect(AcConnectHandler cb, void *arg = 0) {}
    void onDisconnect(AcConnectHandler cb, void *arg = 0) {}
    void onAck(AcAckHandler cb, void *arg = 0) {}
    void onError(AcErrorHandler cb, void *arg = 0) {}
    void onData(AcDataHandler cb, void *arg = 0) {}
    void onPacket(AcPacketHandler cb, void *arg = 0) {}
    void onTimeout(AcTimeoutHandler cb, void *arg = 0) {}
    void onPoll(AcConnectHandler cb, void *arg = 0) {}
    void ackPacket(struct pbuf *pb) {}
    const char *errorToString(err_t error) { return ""; }
    const char *stateToString() { return ""; }
    err_t getCloseError(void) const { return 0; }
};
