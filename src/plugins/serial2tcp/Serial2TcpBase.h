/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <vector>
#include "kfc_fw_config.h"
#include "serial_handler.h"
#include "Serial2TcpConnection.h"

#if DEBUG_SERIAL2TCP && 1
#define __DBGS2T(fmt, ...) { ::printf(PSTR("<%s:%u> "), __DEBUG_FUNCTION__, __LINE__); ::printf((PGM_P)PSTR(fmt), ## __VA_ARGS__); }
#else
#define __DBGS2T(...) ;
#endif

#if DEBUG_SERIAL2TCP && 1
#define __DBGS2T_NVT(fmt, ...) { ::printf(PSTR("<%s:%u> "), __DEBUG_FUNCTION__, __LINE__); ::printf((PGM_P)PSTR(fmt), ## __VA_ARGS__); }
#else
#define __DBGS2T_NVT(...) ;
#endif

class Serial2TcpBase {
public:
    using Serial2TCP = KFCConfigurationClasses::Plugins::Serial2TCP;

protected:
    Serial2TcpBase(Stream &serial, const Serial2TCP::Serial2Tcp_t &config);
public:
    virtual ~Serial2TcpBase();

    void setAuthentication(bool hasAuthentication) {
        _config.authentication = hasAuthentication;
    }
    bool hasAuthentication() const {
        return _config.authentication;
    }

    void setIdleTimeout(uint16_t idleTimeout) {
        _config.idle_timeout = idleTimeout;
    }
    uint16_t getIdleTimeout() const {
        return _config.idle_timeout;
    }

    void setKeepAlive(uint8_t keepAlive) {
        _config.keep_alive = keepAlive;
    }
    uint8_t getKeepAlive() const {
        return _config.keep_alive;
    }

    bool hasAutoConnect() const {
        return _config.auto_connect;
    }

    uint8_t getAutoReconnect() const {
        return _config.auto_reconnect;
    }

    void setAuthenticationParameters(const char *username, const char *password) {
        _username = username;
        _password = password;
    }

    uint16_t getPort() const {
        return _config.port;
    }

    const String &getHostname() const {
        return _hostname;
    }

    void setHostname(const String &hostname) {
        _hostname = hostname;
    }

    const String &getUsername() const {
        return _username;
    }

    const String &getPassword() const {
        return _password;
    }

    static void onSerialData(Stream &client);
    static void handleSerialDataLoop();

    static void handleConnect(void *arg, AsyncClient *client);
    static void handleError(void *arg, AsyncClient *client, int8_t error);
    static void handleData(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time);
    static void handleAck(void *arg, AsyncClient *client, size_t len, uint32_t time);
    static void handlePoll(void *arg, AsyncClient *client);

protected:
    void _setBaudRate(uint32_t baudrate);

    Stream &_getSerial() {
        return _serial;
    }

    Stream &getStream();

    virtual void _onSerialData(Stream &client);

    virtual void _onConnect(AsyncClient *client);
    virtual void _onData(AsyncClient *client, void *data, size_t len);
    // void onError(AsyncClient *client, int8_t error);
    virtual void _onDisconnect(AsyncClient *client, const String &reason);
    // void onTimeOut(AsyncClient *client, uint32_t time);
    // virtual void _onAck(AsyncClient *client, size_t len, uint32_t time);
    // virtual void onPoll(AsyncClient *client);
    void _serialHandlerLoop();

    void _processTcpData(Serial2TcpConnection *conn, const char *data, size_t len);

public:
    virtual void getStatus(Print &output) = 0;
    virtual bool isConnected() const = 0;

    virtual void begin() {}
    virtual void end();

protected:
    void _setClient(SerialHandler::Client *client);
    void _stopClient();
    void _startClient();

private:
    SerialHandler::Client *_client;
    Stream &_serial;
    Serial2TCP::Serial2Tcp_t _config;
    String _hostname;
    String _username;
    String _password;

public:
    static Serial2TcpBase *createInstance(const Serial2TCP::Serial2Tcp_t &cfg, const char *hostname = nullptr);
    static void destroyInstance();

    static Serial2TcpBase *getInstance() {
        return _instance;
    }

private:
    static Serial2TcpBase *_instance;
};
