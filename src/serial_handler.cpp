/**
  Author: sascha_lammers@gmx.de
*/

#include <LoopFunctions.h>
#include <functional>
#include <algorithm>
#include <utility>
#include <HardwareSerial.h>
#include "serial_handler.h"

#if DEBUG_SERIAL_HANDLER
#define DEBUG_SERIAL_HANDLER_BASE           1
#define DEBUG_SERIAL_HANDLER_IO             1
#else
#define DEBUG_SERIAL_HANDLER_BASE           1
#define DEBUG_SERIAL_HANDLER_IO             0
#endif

// serial handler
#if DEBUG_SERIAL_HANDLER_BASE
#define __DBGSH(fmt,...)          { ::printf(PSTR("SH:" fmt "\n"), ##__VA_ARGS__); }
#define __IF_DBGSH(...)           __VA_ARGS__
#else
#define __DBGSH(...)              ;
#define __IF_DBGSH(...)           ;
#endif

// serial handler I/O
#if DEBUG_SERIAL_HANDLER_IO
#define __DBGSHIO(fmt,...)        { ::printf(PSTR("SH:" fmt "\n"), ##__VA_ARGS__); }
#else
#define __DBGSHIO(...)            ;
#endif

NullStream NullSerial;
HardwareSerial Serial0(UART0);
// stream wrapper allows to intercept send and receive on Serial
SerialHandler::Wrapper serialHandler(&Serial0);
Stream &Serial = serialHandler;

#if DEBUG
#if KFC_DEBUG_USE_SERIAL1
StreamWrapper debugStreamWrapperSerial(&Serial1);
Stream &Serial = debugStreamWrapperSerial;
#else
Stream &DebugSerial = serialHandler;
#endif

#else
Stream &DebugSerial = NullSerial;
#endif

namespace SerialHandler {

    //
    // Client
    //

    Client::Client(Callback cb, EventType events) : _cb(cb), _events(events), _rx(0), _tx(0)
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
        _rx.resize(0);
        _tx.flush();
        _tx.resize(0);
    }

    void Client::_checkBufferSize(cbuf &buf, size_t size)
    {
        __IF_DBGSH(auto txTypeStr = (&_rx == &buf ? PSTR("rx") : PSTR("tx")));
        if (buf.size() == 0) {
            buf.resize(Wrapper::kMinBufferSize);
            __DBGSH("client %p %s size=%u data=%u old_size=0", this, txTypeStr, buf.size(), size);
        } else if (size > buf.room()) {
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

    int Client::available()
    {
        return _rx.available();
    }

    int Client::read()
    {
        return _rx.read();
    }

    int Client::peek()
    {
        return _rx.peek();
    }

    size_t Client::write(uint8_t data)
    {
        _checkBufferSize(_tx, sizeof(data));
        serialHandler._txFlag = true;
        return _tx.write(data);
    }

    size_t Client::write(const uint8_t *buffer, size_t size)
    {
        _checkBufferSize(_tx, size);
        serialHandler._txFlag = true;
        return _tx.write((const char *)buffer, size);
    }

    //
    // Wrapper
    //

    Client &Wrapper::addClient(Callback cb, EventType events)
    {
        _clients.emplace_back(cb, events);
        return _clients.back();
    }

    void Wrapper::removeClient(const Client &client)
    {
        _clients.erase(std::remove(_clients.begin(), _clients.end(), client), _clients.end());
    }

    Wrapper::Wrapper(Stream *stream) : StreamWrapper(stream), _txFlag(false)
    {
    }

    Wrapper::~Wrapper()
    {
        end();
    }

    void Wrapper::begin()
    {
        end();
        __DBGSH("begin");
        LoopFunctions::add([this]() {
            this->_loop();
        }, this);
    }

    void Wrapper::end()
    {
        __DBGSH("end");
        LoopFunctions::remove(this);
        _clients.clear();
    }

    void Wrapper::pollSerial()
    {
        serialHandler._pollSerial();
    }

    size_t Wrapper::write(uint8_t data)
    {
        return write(&data, 1);
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
        for(auto &client: _clients) {
            if (src != &client) {
                if (client._hasAny(type)) {
                    auto &rx = client._getRx();
                    client._checkBufferSize(rx, size);
                    rx.write(reinterpret_cast<const char *>(buffer), size);
                }
            }
        }
        // second loop for the callbacks to keep all clients in sync
        for(auto &client: _clients) {
            if (src != &client) {
                auto &rx = client._getRx();
                if (client._hasAny(type) && client._cb && !rx.empty()) {
                    __DBGSHIO("write rx client %p callback %u", &client, rx.available());
                    client._cb(client);
                }
            }
        }
    }

    void Wrapper::_writeClientsTx(Client *src, const uint8_t *buffer, size_t size)
    {
        for(auto &client: _clients) {
            if (src != &client) {
                if (client._hasAny(EventType::WRITE)) {
                    auto &tx = client._getTx();
                    client._checkBufferSize(tx, size);
                    tx.write(reinterpret_cast<const char *>(buffer), size);
                    _txFlag = true;
                }
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
        for(auto &client: _clients) {
            auto &rx = client._getRx();
            if (client._hasAny(EventType::RW) && client._cb && !rx.empty()) {
                __DBGSHIO("transmit rx client %p callback %u", &client, rx.available());
                client._cb(client);
                client._resizeBufferMinSize(rx);
            }
        }
    }

    void Wrapper::_transmitClientsTx()
    {
        for(auto &client: _clients) {
            auto &tx = client._getTx();
            if (tx.available()) {
                uint8_t buf[Wrapper::kMinBufferSize / 2];
                size_t len;
                while((len = tx.read(reinterpret_cast<char *>(buf), sizeof(buf))) != 0) {
                    __DBGSHIO("transmit tx client %p size %u", &client, len);
                    _writeClientsRx(&client, buf, len, EventType::READ);
                }
            }
            client._resizeBufferMinSize(tx);
        }
        _txFlag = false;
    }

    void Wrapper::_loop()
    {
        _pollSerial();
        _transmitClientsRx();
        _transmitClientsTx();
    }

};
