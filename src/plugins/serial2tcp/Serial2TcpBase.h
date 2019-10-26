/**
 * Author: sascha_lammers@gmx.de
 */

#if SERIAL2TCP

#pragma once

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <vector>
#include "kfc_fw_config.h"
#include "Serial2TcpConnection.h"

class Serial2TcpBase {
protected:
    Serial2TcpBase(Stream &serial, uint8_t serialPort);
public:
    virtual ~Serial2TcpBase();

    void setAuthentication(bool hasAuthentication) {
        // _hasAuthentication = hasAuthentication;
        // TODO authentication not supported yet
        _hasAuthentication = false;
    }

    bool hasAuthentication() const {
        return _hasAuthentication;
    }

    void setIdleTimeout(uint16_t idleTimeout) {
        _idleTimeout = idleTimeout;
    }

    uint16_t getIdleTimeout() const {
        return _idleTimeout;
    }

    void setKeepAlive(uint8_t keepAlive) {
        _keepAlive = keepAlive;
    }

    bool getAutoConnect() const {
        return _autoConnect;
    }

    uint8_t getAutoReconnect() const {
        return _autoReconnect;
    }

    uint8_t getKeepAlive() const {
        return _keepAlive;
    }

    void setAuthenticationParameters(const char *username, const char *password) {
        _username = username;
        _password = password;
    }

    uint16_t getPort() const {
        return _port;
    }

    String getHost() const {
        return _host;
    }

    String getUsername() const {
        return _username;
    }

    String getPassword() const {
        return _password;
    }

    void setConnectionParameters(const char *host, uint16_t port, bool autoConnect, uint8_t autoReconnect);

    static void onSerialData(uint8_t type, const uint8_t *buffer, size_t len);
    static void handleSerialDataLoop();

    static void handleConnect(void *arg, AsyncClient *client);
    static void handleError(void *arg, AsyncClient *client, int8_t error);
    static void handleData(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time);
    static void handleAck(void *arg, AsyncClient *client, size_t len, uint32_t time);
    static void handlePoll(void *arg, AsyncClient *client);

    static bool isServer(const ConfigFlags &flags) {
        return (flags.serial2TCPMode == SERIAL2TCP_MODE_SECURE_SERVER || flags.serial2TCPMode == SERIAL2TCP_MODE_UNSECURE_SERVER);
    }

    static bool isTLS(const ConfigFlags &flags) {
        return (flags.serial2TCPMode == SERIAL2TCP_MODE_SECURE_SERVER || flags.serial2TCPMode == SERIAL2TCP_MODE_SECURE_CLIENT);
    }

protected:
    void _setBaudRate(uint32_t rate);

    Stream &_getSerial() const {
        return _serial;
    }

    uint8_t _getSerialPort() const {
        return _serialPort;
    }

    virtual void _onSerialData(uint8_t type, const uint8_t *buffer, size_t len);

    virtual void _onConnect(AsyncClient *client);
    virtual void _onData(AsyncClient *client, void *data, size_t len);
    // void onError(AsyncClient *client, int8_t error);
    virtual void _onDisconnect(AsyncClient *client, const __FlashStringHelper *reason);
    // void onTimeOut(AsyncClient *client, uint32_t time);
    // virtual void _onAck(AsyncClient *client, size_t len, uint32_t time);
    // virtual void onPoll(AsyncClient *client);
    void _serialHandlerLoop();

    void _processData(Serial2TcpConnection *conn, const char *data, size_t len);

    virtual size_t _serialWrite(Serial2TcpConnection *conn, const char *data, size_t len);
    size_t _serialWrite(Serial2TcpConnection *conn, char data);
    size_t _serialPrintf_P(Serial2TcpConnection *conn, PGM_P format, ...);

public:
    virtual void getStatus(PrintHtmlEntitiesString &output) = 0;

    virtual void begin() = 0;
    virtual void end() = 0;

private:
    Stream &_serial;
    uint8_t _serialPort;
    uint8_t _hasAuthentication: 1;
    uint8_t _autoConnect: 1;
    uint8_t _autoReconnect;
    uint16_t _idleTimeout;
    uint8_t _keepAlive;
    String _host;
    uint16_t _port;
    String _username;
    String _password;

public:
    static Serial2TcpBase *createInstance();
    static void destroyInstance();

    static Serial2TcpBase *getInstance() {
        return _instance;
    }

private:
    static Serial2TcpBase *_instance;

public:
#if IOT_DIMMER_MODULE
    static bool _resetAtmega;
#endif
#if DEBUG
    static bool _debugOutput;
#endif
};

#endif
