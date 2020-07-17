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

NullStream NullSerial;
HardwareSerial Serial0(UART0);
// stream wrapper allows to intercept send and receive on Serial
StreamWrapper streamWrapperSerial(&Serial0);
Stream &Serial = streamWrapperSerial;

#if DEBUG
Stream &DebugSerial = streamWrapperSerial;
#else
// we cannot change references
// using SerialWrapper allows to change to streamWrapperSerial later
SerialWrapper debugWrapper(NullSerial);
Stream &DebugSerial = debugWrapper;
#endif

#if SERIAL_HANDLER

//SerialWrapper serialWrapper();
SerialHandler serialHandler(streamWrapperSerial);

SerialHandler::SerialHandler(StreamWrapper &wrapper) : _wrapper(wrapper)
{
    begin();
}

void SerialHandler::clear()
{
    _handlers.clear();
}

void SerialHandler::begin()
{
    _debug_println();
    LoopFunctions::add(SerialHandler::serialLoop);
    _wrapper.add(this);
}

void SerialHandler::end()
{
    _debug_println();
    _wrapper.remove(this);
    LoopFunctions::remove(SerialHandler::serialLoop);
}

void SerialHandler::addHandler(SerialHandlerCallback_t callback, uint8_t flags)
{
    _debug_printf_P(PSTR("SerialHandler::addHandler(%p, rx %d tx %d remoterx %d localtx %d buffered %d)\n"), callback, (flags & RECEIVE ? 1 : 0), (flags & TRANSMIT ? 1 : 0), (flags & REMOTE_RX ? 1 : 0), (flags & LOCAL_TX ? 1 : 0), 0);
    _handlers.emplace_back(callback, flags);
}

void SerialHandler::removeHandler(SerialHandlerCallback_t callback)
{
    _debug_printf_P(PSTR("SerialHandler::removeHandler(%p)\n"), callback);
    _handlers.erase(std::remove(_handlers.begin(), _handlers.end(), callback), _handlers.end());
}

void SerialHandler::serialLoop()
{
    serialHandler._serialLoop();
}

size_t SerialHandler::write(uint8_t data)
{
    writeToTransmit(TRANSMIT, nullptr, &data, sizeof(data));
    return sizeof(data);
}

size_t SerialHandler::write(const uint8_t *buffer, size_t size)
{
    writeToTransmit(TRANSMIT, nullptr, buffer, size);
    return size;
}

void SerialHandler::_serialLoop()
{
    auto &serial = *_wrapper.getInput();
    uint8_t buf[256];
    int avail;

    while((avail = serial.available()) > 0) {
        if (avail >= (int)sizeof(buf)) {
            avail = sizeof(buf) - 1;
        }
        int len = serial.readBytes(buf, avail);
        writeToReceive(RECEIVE, buf, len);
    }
}

void SerialHandler::writeToTransmit(SerialDataType_t type, const uint8_t *buffer, size_t len)
{
    // Serial.printf_P(PSTR("SerialHandler::writeToTransmit(%d, %p, %u)\n"), type, buffer, len);
    writeToTransmit(type, nullptr, buffer, len);
}

void SerialHandler::writeToTransmit(SerialDataType_t type, SerialHandlerCallback_t callback, const uint8_t *buffer, size_t len)
{
    static bool locked = false;
    if (locked) {
        _debug_printf_P(PSTR("SerialHandler::writeToTransmit(%d, %p, len %d, locked %d)\n"), type, callback, len, locked);
        return;
    }
    locked = true;
    // _debug_printf_P(PSTR("SerialHandler::writeToTransmit(%d, %p, len %d, locked %d)\n"), type, callback, len, locked);
    for(const auto &handler: _handlers) {
        // _debug_printf_P(PSTR("SerialHandler::writeToTransmit(): Handler %p flags %02x match %d\n"), handler.cb, handler.flags, ((handler.flags & type) && handler.cb != callback));
        if (handler.hasType(type) && handler.getCallback() != callback) {
            handler.invoke(type, buffer, len);
        }
    }
    locked = false;
}

void SerialHandler::writeToReceive(SerialDataType_t type, const uint8_t *buffer, size_t len)
{
    writeToReceive(type, nullptr, buffer, len);
}

// sends to rx callbacks (=receiving data from serial)
void SerialHandler::writeToReceive(SerialDataType_t type, SerialHandlerCallback_t callback, const uint8_t *buffer, size_t len)
{
    static bool locked = false;
    if (locked) {
        _debug_printf_P(PSTR("SerialHandler::writeToReceive(%d, %p, len %d, locked %d)\n"), type, callback, len, locked);
        return;
    }
    // _debug_printf_P(PSTR("SerialHandler::writeToReceive(%d, %p, len %d, locked %d)\n"), type, callback, len, locked);
    locked = true;
    if (len) {
        // _debug_printf_P(PSTR("Raw data, len %d, type %s\n"), len, (type == REMOTE_RX ? "remoteRX" : "RX"));
        for(auto iterator = _handlers.rbegin(); iterator != _handlers.rend(); ++iterator) {
            const auto &handler = *iterator;
            // _debug_printf_P(PSTR("SerialHandler::writeToReceive(): Handler %p flags %02x match %d\n"), handler.cb, handler.flags, ((handler.flags & type) && callback != handler.cb));
            if (handler.hasType(type) && handler.getCallback() != callback) {
                handler.invoke(type, buffer, len);
            }
        }
    }
    locked = false;
}

void SerialHandler::receivedFromRemote(SerialHandlerCallback_t callback, const uint8_t *buffer, size_t len)
{
    // _debug_printf_P(PSTR("SerialHandler::receivedFromRemote(%p, %d)\n"), callback, len);
    _wrapper.write(buffer, len);
    writeToReceive(REMOTE_RX, callback, buffer, len);
}


SerialWrapper::SerialWrapper(Stream &serial) : _serial(serial)
{
}

size_t SerialWrapper::write(uint8_t data)
{
    size_t written = _serial.write(data);
    if (written) {
        SerialHandler::getInstance().writeToTransmit(SerialHandler::LOCAL_TX, &data, sizeof(data));
    }
    return written;
}

size_t SerialWrapper::write(const uint8_t *data, size_t len)
{
    size_t written = _serial.write(data, len);
    if (written) {
        SerialHandler::getInstance().writeToTransmit(SerialHandler::LOCAL_TX, data, written);
    }
    return written;
}

size_t SerialWrapper::write(const char *buffer)
{
    return write(reinterpret_cast<const uint8_t *>(buffer), strlen(buffer));
}

void SerialWrapper::flush()
{
    _serial.flush();
    // SerialHandler::_flush();
}

int SerialWrapper::available()
{
    return _serial.available();
}

int SerialWrapper::read()
{
    return _serial.read();
}

int SerialWrapper::peek()
{
    return _serial.peek();
}

#endif
