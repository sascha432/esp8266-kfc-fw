/**
  Author: sascha_lammers@gmx.de
*/

#include <LoopFunctions.h>
#include <functional>
#include <algorithm>
#include <utility>
#include <vector>
#include <HardwareSerial.h>
#include "serial_handler.h"

#if DEBUG_SERIAL_HANDLER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
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

    Client::Client(Callback cb, EventType events) : _cb(cb), _events(events), _rx(Wrapper::kMinBufferSize), _tx(Wrapper::kMinBufferSize)
    {
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
        serialHandler._txFlag = true;
        return _tx.write(data);
    }

    size_t Client::write(const uint8_t *buffer, size_t size)
    {
        serialHandler._txFlag = true;
        return _tx.write((const char *)buffer, size);
    }

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
        DEBUG_SH2("begin");
        LoopFunctions::add([this]() {
            this->_loop();
        }, this);
    }

    void Wrapper::end()
    {
        DEBUG_SH2("end");
        LoopFunctions::remove(loop);
        _clients.clear();
    }

    size_t Wrapper::write(uint8_t data)
    {
        return write(&data, 1);
    }

    size_t Wrapper::write(const uint8_t *buffer, size_t size)
    {
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
                if (client._has(type)) {
                    auto &rx = client._getRx();
                    if (rx.room() < size && rx.size() < kMaxBufferSize) {
                        rx.resizeAdd(kAddBufferSize);
                        DEBUG_SH2("rx resize %u", rx.size());
                    }
                    rx.write(reinterpret_cast<const char *>(buffer), size);
                }
            }
        }
    }

    void Wrapper::_writeClientsTx(Client *src, const uint8_t *buffer, size_t size)
    {
        for(auto &client: _clients) {
            if (src != &client) {
                if (client._has(EventType::WRITE)) {
                    auto &tx = client._getTx();
                    if (tx.room() < size && tx.size() < kMaxBufferSize) {
                        tx.resizeAdd(kAddBufferSize);
                        DEBUG_SH2("tx resize %u", tx.size());
                    }
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
            uint8_t buf[64];
            size_t len = 0;
            auto ptr = buf;
            while (serial.available() && len < sizeof(buf)) {
                *ptr++ = serial.read();
                len++;
            }
            _writeClientsRx(nullptr, buf, len, EventType::READ);
        }
    }

    void Wrapper::_transmitClientsRx()
    {
        for(auto &client: _clients) {
            auto &rx = client._getRx();
            if (client._has(EnumHelper::Bitset::all(EventType::READ, EventType::WRITE)) && client._cb && !rx.empty()) {
                client._cb(client);
                if (rx.empty() && rx.size() > Wrapper::kMinBufferSize) {
                    rx.resize(Wrapper::kMinBufferSize);
                }
            }
        }
    }

    void Wrapper::_transmitClientsTx()
    {
        for(auto &client: _clients) {
            auto &tx = client._getTx();
            if (tx.available()) {
                uint8_t buf[64];
                size_t len;
                while((len = tx.read(reinterpret_cast<char *>(buf), sizeof(buf))) != 0) {
                    StreamWrapper::write(buf, len);
                    _writeClientsRx(&client, buf, len, EventType::READ);
                }
            }
            if (tx.size() > Wrapper::kMinBufferSize) {
                tx.resize(Wrapper::kMinBufferSize);
            }
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
