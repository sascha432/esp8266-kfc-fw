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
        _allocateBuffers();
    }

    void Client::start(EventType events)
    {
        _events = events;
        _rx.flush();
        _tx.flush();
        _allocateBuffers();
    }

    void Client::stop()
    {
        _events = EventType::NONE;
        _freeBuffers();
    }

    void Client::_checkBufferSize(cbuf &buf, size_t size)
    {
        if (buf.size() == Wrapper::kInitBufferSize) {
            buf.resize(Wrapper::kMinBufferSize);
        }
        else if (size > buf.room()) {
            if (buf.size() < Wrapper::kMaxBufferSize) {
                buf.resizeAdd(Wrapper::kAddBufferSize);
            }
        }
    }

    void Client::_resizeBufferMinSize(cbuf &buf)
    {
        if (buf.empty() && buf.size() > Wrapper::kMinBufferSize) {
            buf.resize(Wrapper::kMinBufferSize);
        }
    }

    size_t Client::write(uint8_t data)
    {
        _checkBufferSize(_tx, sizeof(data));
        serialHandler._txFlag = true;
        return _tx.write(data);
    }

    size_t Client::write(const uint8_t *buffer, size_t size)
    {
        __DBG_validatePointerCheck(buffer, VP_HS);
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
                    __DBG_validatePointerCheck(client.get(), VP_HS);
                    return client.get() == ptr;
                }), _clients.end());
            }
        });
    }

    size_t Wrapper::write(const uint8_t *buffer, size_t size)
    {
        if (_txFlag) {
            _transmitClientsTx(); // check if any other data is queued
        }
        const size_t written = StreamWrapper::write(buffer, size);
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
                clientPtr->_cb(*clientPtr);
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
            _writeClientsRx(nullptr, buf, len, EventType::READ);
        }
    }

    void Wrapper::_transmitClientsRx()
    {
        for(const auto &clientPtr: _clients) {
            if (clientPtr) {
                __DBG_validatePointerCheck(clientPtr.get(), VP_HS);
                auto &client = *clientPtr;
                auto &rx = client._getRx();
                if (client._hasAny(EventType::RW) && client._cb && !rx.empty()) {
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
                __DBG_validatePointerCheck(clientPtr.get(), VP_HS);
                auto &tx = clientPtr->_getTx();
                if (tx.available()) {
                    uint8_t buf[Wrapper::kMinBufferSize / 2];
                    size_t len;
                    while((len = tx.read(reinterpret_cast<char *>(buf), sizeof(buf))) != 0) {
                        _writeClientsRx(clientPtr.get(), buf, len, EventType::READ);
                    }
                }
                clientPtr->_resizeBufferMinSize(tx);
            }
        }
        _txFlag = false;
    }

    void Wrapper::_loop()
    {
        #if ESP32
            bool deleteWdt = false;
            esp_err_t err = esp_task_wdt_status(NULL);
            if (err == ESP_ERR_NOT_FOUND) {
                if ((err = esp_task_wdt_add(NULL)) != ESP_OK) {
                    if (err != ESP_ERR_INVALID_ARG) {
                        __DBG_printf_E("esp_task_wdt_add failed err=%x", err);
                    }
                }
                else {
                    deleteWdt = true;
                }
            }
        #endif
        MUTEX_LOCK_BLOCK(_lock) {
            _pollSerial();
            _transmitClientsRx();
            _transmitClientsTx();
        }
        #if ESP32
            if (deleteWdt) {
                esp_task_wdt_delete(NULL);
            }
        #endif
    }

}
