/**
  Author: sascha_lammers@gmx.de
*/

#include <LoopFunctions.h>
#include <functional>
#include <algorithm>
#include <utility>
#include <vector>
#include "serial_handler.h"

#if DEBUG_SERIAL_HANDLER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if !SERIAL_HANDLER

StreamWrapper MySerialWrapper(&KFC_SERIAL_PORT, &KFC_SERIAL_PORT);
Stream &MySerial = MySerialWrapper;
Stream &DebugSerial = MySerialWrapper;

#else

static SerialWrapper _MySerial(KFC_SERIAL_PORT);
StreamWrapper MySerialWrapper(&_MySerial, &_MySerial);
Stream &MySerial = MySerialWrapper;
Stream &DebugSerial = MySerialWrapper;
SerialHandler serialHandler(_MySerial);

SerialHandler::SerialHandler(SerialWrapper &wrapper) : _wrapper(wrapper) {
}

void SerialHandler::clear() {
    _handlers.clear();
}

void SerialHandler::begin() {
    LoopFunctions::add([this]() {
        this->serialLoop();
    }, reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
}

void SerialHandler::end() {
    LoopFunctions::remove(reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
}

void SerialHandler::addHandler(SerialHandlerCb callback, uint8_t flags) {

    _debug_printf_P(PSTR("SerialHandler::addHandler(%p, rx %d tx %d remoterx %d localtx %d buffered %d)\n"), callback, (flags & RECEIVE ? 1 : 0), (flags & TRANSMIT ? 1 : 0), (flags & REMOTE_RX ? 1 : 0), (flags & LOCAL_TX ? 1 : 0), 0);
    _handlers.push_back({callback, flags});
}

void SerialHandler::removeHandler(SerialHandlerCb callback) {

    _debug_printf_P(PSTR("SerialHandler::removeHandler(%p)\n"), callback);
    _handlers.erase(std::remove_if(_handlers.begin(), _handlers.end(), [&](SerialHandlers &handlers) {
        if (handlers.cb == callback) {
            return true;
        }
        return false;
    }), _handlers.end());
}

void SerialHandler::serialLoop() {

    while(_wrapper.available()) {
        uint8_t buf[128];
        uint8_t i;
        for(i = 0; i < sizeof(buf) && _wrapper.available(); i++) {
            buf[i] = _wrapper.read();
        }
        writeToReceive(RECEIVE, buf, i);
    }
}

void SerialHandler::writeToTransmit(SerialDataType_t type, const uint8_t *buffer, size_t len) {
    writeToTransmit(type, nullptr, buffer, len);
}

void SerialHandler::writeToTransmit(SerialDataType_t type, SerialHandlerCb callback, const uint8_t *buffer, size_t len) {
    static bool locked = false;
    if (locked) {
        _debug_printf_P(PSTR("SerialHandler::writeToTransmit(%d, %p, len %d, locked %d)\n"), type, callback, len, locked);
        return;
    }
    locked = true;
    // _debug_printf_P(PSTR("SerialHandler::writeToTransmit(%d, %p, len %d, locked %d)\n"), type, callback, len, locked);
    for(const auto &handler: _handlers) {
        // _debug_printf_P(PSTR("SerialHandler::writeToTransmit(): Handler %p flags %02x match %d\n"), handler.cb, handler.flags, ((handler.flags & type) && handler.cb != callback));
        if ((handler.flags & type) && handler.cb != callback) {
            (*(handler.cb))(type, buffer, len);
        }
    }
    locked = false;
}

void SerialHandler::writeToReceive(SerialDataType_t type, const uint8_t *buffer, size_t len) {
    writeToReceive(type, nullptr, buffer, len);
}

// sends to rx callbacks (=receiving data from serial)
void SerialHandler::writeToReceive(SerialDataType_t type, SerialHandlerCb callback, const uint8_t *buffer, size_t len) {
    static bool locked = false;
    if (locked) {
        _debug_printf_P(PSTR("SerialHandler::writeToReceive(%d, %p, len %d, locked %d)\n"), type, callback, len, locked);
        return;
    }
    // _debug_printf_P(PSTR("SerialHandler::writeToReceive(%d, %p, len %d, locked %d)\n"), type, callback, len, locked);
    locked = true;
    if (len) {
        // _debug_printf_P(PSTR("Raw data, len %d, type %s\n"), len, (type == REMOTE_RX ? "remoteRX" : "RX"));
        for(auto it = _handlers.rbegin(); it != _handlers.rend(); ++it) {
            const auto &handler = *it;
            // _debug_printf_P(PSTR("SerialHandler::writeToReceive(): Handler %p flags %02x match %d\n"), handler.cb, handler.flags, ((handler.flags & type) && callback != handler.cb));
            if ((handler.flags & type) && callback != handler.cb) {
                (*(handler.cb))(type, (const uint8_t *)buffer, len);
            }
        }
    }
    locked = false;
}

void SerialHandler::receivedFromRemote(SerialHandlerCb callback, const uint8_t *buffer, size_t len) {
    // _debug_printf_P(PSTR("SerialHandler::receivedFromRemote(%p, %d)\n"), callback, len);
    _wrapper.getSerial().write(buffer, len);
    writeToReceive(REMOTE_RX, callback, buffer, len);
}



SerialWrapper::SerialWrapper(Stream &serial) : _serial(serial) {
}


size_t SerialWrapper::write(uint8_t data) {
    size_t written = _serial.write(data);
    if (written) {
        serialHandler.writeToTransmit(SerialHandler::LOCAL_TX, &data, written);
    }
    return written;
}

size_t SerialWrapper::write(const uint8_t *data, size_t len) {
    size_t written = _serial.write(data, len);
    if (written) {
        serialHandler.writeToTransmit(SerialHandler::LOCAL_TX, data, written);
    }
    return written;
}

size_t SerialWrapper::write(const char *buffer) {
    return write((const uint8_t *)buffer, strlen(buffer));
}

void SerialWrapper::flush() {
    _serial.flush();
    // SerialHandler::_flush();
}

#endif
