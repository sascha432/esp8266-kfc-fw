/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <LoopFunctions.h>
#include <functional>
#include <algorithm>
#include <utility>
#include <HardwareSerial.h>
#if ESP8266
#include <interrupts.h>
#endif
#if ESP32
#include <esp32-hal-uart.h>
#endif
#include "serial_handler.h"

#if DEBUG_SERIAL_HANDLER
#    define DEBUG_SERIAL_HANDLER_BASE 1
#    define DEBUG_SERIAL_HANDLER_IO   1
#else
#    define DEBUG_SERIAL_HANDLER_BASE 0
#    define DEBUG_SERIAL_HANDLER_IO   0
#endif

// number of lines to keep during the boot face. additional data is discarded
#ifndef DEBUG_PREBOOT_HISTORY
#    define DEBUG_PREBOOT_HISTORY 0
#endif

// serial handler
#if DEBUG_SERIAL_HANDLER_BASE
#    define __DBGSH(fmt, ...)                              \
        {                                                  \
            ::printf(PSTR("SH:" fmt "\n"), ##__VA_ARGS__); \
        }
#    define __IF_DBGSH(...) __VA_ARGS__
#else
#    define __DBGSH(...)    ;
#    define __IF_DBGSH(...) ;
#endif

// serial handler I/O
#if DEBUG_SERIAL_HANDLER_IO
#    define __DBGSHIO(fmt, ...)                            \
        {                                                  \
            ::printf(PSTR("SH:" fmt "\n"), ##__VA_ARGS__); \
        }
#else
#    define __DBGSHIO(...) ;
#endif

NullStream NullSerial;
HardwareSerial Serial0(UART0);
// stream wrapper allows to intercept send and receive on Serial
SerialHandler::Wrapper serialHandler(&Serial0);
Stream &Serial = serialHandler;
// Stream &Serial = Serial0;

#if DEBUG
#    if KFC_DEBUG_USE_SERIAL1
        StreamWrapper debugStreamWrapper(&Serial1);
        Stream &Serial = debugStreamWrapper;
#    elif 1
        StreamWrapper debugStreamWrapper(&Serial);
        // Stream &DebugSerial = Serial0;
        Stream &DebugSerial = debugStreamWrapper;
#    else
        Stream &DebugSerial = serialHandler;
#    endif
#else
    Stream &DebugSerial = NullSerial;
#endif

namespace SerialHandler {

    //
    // Client
    //

    Client::Client() :
        _events(EventType::NONE),
        _rx(Wrapper::kInitBufferSize),
        _tx(Wrapper::kInitBufferSize)
    {
    }

    Client::Client(const Callback &cb, EventType events) :
        _cb(cb),
        _events(events),
        _rx(Wrapper::kInitBufferSize),
        _tx(Wrapper::kInitBufferSize)
    {
        __DBGSH("create client %p", this);
        _allocateBuffers();
    }

    void Client::start(EventType events)
    {
        __DBGSH("client %p start %02x", this, events);
        _events = events;
        _rx.flush();
        _tx.flush();
        _allocateBuffers();
    }

    void Client::stop()
    {
        __DBGSH("client %p stop");
        _events = EventType::NONE;
        _freeBuffers();
    }

    void Client::_checkBufferSize(cbuf &buf, size_t size)
    {
        __IF_DBGSH(auto txTypeStr = (&_rx == &buf ? PSTR("rx") : PSTR("tx")));
        if (buf.size() == Wrapper::kInitBufferSize) {
            buf.resize(Wrapper::kMinBufferSize);
            __DBGSH("client %p %s size=%u data=%u old_size=0", this, txTypeStr, buf.size(), size);
        }
        else if (size > buf.room()) {
            if (buf.size() < Wrapper::kMaxBufferSize) {
                __IF_DBGSH(size_t room = buf.room());
                buf.resizeAdd(Wrapper::kAddBufferSize);
                __DBGSH("client %p %s size=%u data=%u room=%u", this, txTypeStr, buf.size(), size, room);
            }
            else {
                __IF_DBGSH(
                    if (buf.size() == Wrapper::kRealMaxBufferSize) {
                        buf.resize(Wrapper::kRealMaxBufferSize + 1); // suppress multiple debug messages
                        __DBGSH("client %p %s full size=%u data=%u room=%u", this, txTypeStr, buf.size(), size, buf.room());
                    }
                );
                __DBGSH("client %p %s full size=%u data=%u room=%u", this, txTypeStr, buf.size(), size, buf.room());
            }
        }
    }

    void Client::_resizeBufferMinSize(cbuf &buf)
    {
        __IF_DBGSH(
            auto bufSize = buf.size();
        );
        if (buf.empty() && buf.size() > Wrapper::kMinBufferSize) {
            buf.resize(Wrapper::kMinBufferSize);
        }
        __IF_DBGSH(
            if (bufSize != buf.size()) {
                __DBGSH("client %p %s size=%u from=%u", this, (&_rx == &buf ? PSTR("rx") : PSTR("tx")), buf.size(), bufSize);
            }
        );
    }

    size_t Client::write(uint8_t data)
    {
        _checkBufferSize(_tx, sizeof(data));
        serialHandler._txFlag = true;
        return _tx.write(data);
    }

    size_t Client::write(const uint8_t *buffer, size_t size)
    {
        __DBG_validatePointer(buffer, VP_HS);
        _checkBufferSize(_tx, size);
        serialHandler._txFlag = true;
        return _tx.write((const char *)buffer, size);
    }


    void Client::_allocateBuffers()
    {
        if (_hasAny(EventType::READ)) {
            _rx.resize(Wrapper::kMinBufferSize);
        }
        // tx starts with 0. not all clients transmit data
    }

    void Client::_freeBuffers()
    {
        _rx.flush();
        _rx.resize(Wrapper::kInitBufferSize);
        _tx.flush();
        _tx.resize(Wrapper::kInitBufferSize);
    }

    //
    // Wrapper
    //

    Wrapper::Wrapper(Stream *stream) :
        StreamWrapper(stream),
        _txFlag(false)
    {
    }

    void Wrapper::removeClient(const Client &client)
    {
        auto ptr = std::addressof(client);
        // remove outside interrupts
        LoopFunctions::callOnce([ptr, this]() {
            MUTEX_LOCK_BLOCK(_lock) {
                _clients.erase(std::remove_if(_clients.begin(), _clients.end(), [ptr](const ClientPtr &client) {
                    __DBG_validatePointer(client.get(), VP_HS);
                    return client.get() == ptr;
                }), _clients.end());
            }
        });
    }

    size_t Wrapper::write(const uint8_t *buffer, size_t size)
    {
        __DBGSHIO("wrapper write %u", size);
        if (_txFlag) {
            _transmitClientsTx(); // check if any other data is queued
        }
        size_t written = StreamWrapper::write(buffer, size);
        _writeClientsRx(nullptr, buffer, written, EventType::WRITE);
        return written;
    }

    void Wrapper::_writeClientsRx(Client *src, const uint8_t *buffer, size_t size, EventType type)
    {
        for(const auto &clientPtr: _clients) {
            if (clientPtr && (src != clientPtr.get()) && __DBG_validatePointer(clientPtr.get(), VP_HS)->_hasAny(type)) {
                auto &rx = clientPtr->_getRx();
                clientPtr->_checkBufferSize(rx, size);
                rx.write(reinterpret_cast<const char *>(buffer), size);
                #if ESP32
                    esp_task_wdt_reset();
                #endif
            }
        }
        // second loop for the callbacks to keep all clients in sync
        for(const auto &clientPtr: _clients) {
            if (clientPtr && (src != clientPtr.get()) && __DBG_validatePointer(clientPtr.get(), VP_HS)->_hasAny(type) && clientPtr->_cb && !clientPtr->_getRx().empty()) {
                __DBGSHIO("write rx client %p callback %u", clientPtr.get(), clientPtr->_getRx().available());
                clientPtr->_cb(*clientPtr);
                #if ESP32
                    esp_task_wdt_reset();
                #endif
            }
        }
    }

    void Wrapper::_writeClientsTx(Client *src, const uint8_t *buffer, size_t size)
    {
        for(const auto &clientPtr: _clients) {
            if (clientPtr && (src != clientPtr.get()) && __DBG_validatePointer(clientPtr.get(), VP_HS)->_hasAny(EventType::WRITE)) {
                auto &tx = clientPtr->_getTx();
                clientPtr->_checkBufferSize(tx, size);
                tx.write(reinterpret_cast<const char *>(buffer), size);
                _txFlag = true;
                #if ESP32
                    esp_task_wdt_reset();
                #endif
            }
        }
    }

    void Wrapper::_pollSerial()
    {
        auto &serial = *getInput();
        while(serial.available()) {
            uint8_t buf[Wrapper::kMinBufferSize / 2];
            size_t len = 0;
            auto ptr = buf;
            while (serial.available() && len < sizeof(buf)) {
                *ptr++ = serial.read();
                len++;
            }
            __DBGSHIO("poll %u", len);
            _writeClientsRx(nullptr, buf, len, EventType::READ);
        }
    }

    void Wrapper::_transmitClientsRx()
    {
        for(const auto &clientPtr: _clients) {
            if (clientPtr) {
                __DBG_validatePointer(clientPtr.get(), VP_HS);
                auto &client = *clientPtr;
                auto &rx = client._getRx();
                if (client._hasAny(EventType::RW) && client._cb && !rx.empty()) {
                    __DBGSHIO("transmit rx client %p callback %u", &client, rx.available());
                    client._cb(client);
                    client._resizeBufferMinSize(rx);
                    #if ESP32
                        esp_task_wdt_reset();
                    #endif
                }
            }
        }
    }

    void Wrapper::_transmitClientsTx()
    {
        for(const auto &clientPtr: _clients) {
            if (clientPtr) {
                __DBG_validatePointer(clientPtr.get(), VP_HS);
                auto &tx = clientPtr->_getTx();
                if (tx.available()) {
                    uint8_t buf[Wrapper::kMinBufferSize / 2];
                    size_t len;
                    while((len = tx.read(reinterpret_cast<char *>(buf), sizeof(buf))) != 0) {
                        __DBGSHIO("transmit tx client %p size %u", &client, len);
                        _writeClientsRx(clientPtr.get(), buf, len, EventType::READ);
                    }
                }
                clientPtr->_resizeBufferMinSize(tx);
            }
        }
        _txFlag = false;
    }

}
