/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// any HLK LD24xx radar sensor that is configured over its serial port
#ifndef IOT_SENSOR_HAVE_LD24XX
#    define IOT_SENSOR_HAVE_LD24XX (IOT_SENSOR_HAVE_LD2420 || IOT_SENSOR_HAVE_LD2450)
#endif

#if IOT_SENSOR_HAVE_LD24XX

// Serial port and command frames of the HLK LD24xx radar sensors
//
// The hardware UARTs, fixed pins: the sensor's RX on GPIO2 (UART1 TX, TX only), its TX on GPIO13 (OT1 of the LD2420,
// TX of the LD2450). UART0 is the serial console and receives the sensor only while the port is open (while a command
// runs): the RX signal is switched from GPIO3 to GPIO13, the same pin swap as Serial.swap(). The serial console is
// paused meanwhile, its input is ignored and its output is buffered (kConsoleHoldSize) and sent when the port is
// closed. The console baud rate is switched to the one of the sensor while the port is open, 115200 (LD2420 firmware
// 1.5.3 and later) or 256000 (LD2450).
//
// SoftwareSerial does not work at these baud rates: bit errors and lost replies, and the frames have no checksum.
//
// Protocol, little endian
//   command: FD FC FB FA <length:2> <command:2> <data...> 04 03 02 01, length = 2 + data length
//   reply:   FD FC FB FA <length:2> <command | 0x100:2> <error:2> <data...> 04 03 02 01
// https://github.com/esphome/esphome/tree/dev/esphome/components/ld2420
// https://github.com/esphome/esphome/tree/dev/esphome/components/ld2450

#if !ESP8266
#    error the LD24xx serial port supports ESP8266 only (UART0 pin swap)
#endif

#include <Arduino_compat.h>

namespace LD24xx {

    // console output that is buffered while UART0 receives the sensor, the rest is dropped
    constexpr uint16_t kConsoleHoldSize = 1024;

    // UART0 RX swapped to GPIO13, UART1 TX
    constexpr uint8_t kRxPin = 13;
    constexpr uint8_t kTxPin = 2;

}

// collects the console output while UART0 receives the sensor
class LD24xxConsoleHold : public Stream {
public:
    LD24xxConsoleHold();

    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    // sends the collected output to the console
    void release(Print &output);

private:
    String _buffer;
    uint32_t _dropped;
};

// Hardware UARTs: TX is UART1 on GPIO2, RX is UART0 switched from GPIO3 to GPIO13 while the object exists. The serial
// console (serialHandler) gets no input meanwhile and its output is collected by LD24xxConsoleHold
class LD24xxHardwareUart {
public:
    LD24xxHardwareUart(uint32_t baud);
    ~LD24xxHardwareUart();

    // returns -1 if no byte is available
    int read();
    void write(const uint8_t *data, size_t length);
    // overruns and framing errors
    uint16_t getErrors() const;

private:
    void _pauseConsole();
    void _resumeConsole();

private:
    LD24xxConsoleHold _hold;
    Stream *_input;
    uint32_t _consoleBaud;
    uint32_t _baud;
    uint16_t _errors;
    bool _holding;
};

inline uint16_t LD24xxHardwareUart::getErrors() const
{
    return _errors;
}

class LD24xxSerial {
public:
    static constexpr uint8_t kMaxFrameLength = 64;
    // the largest command data: the zone areas of the LD2450
    static constexpr uint8_t kMaxCommandData = 26;

    static constexpr uint16_t kCmdDisableConfig = 0x00fe;
    static constexpr uint16_t kCmdEnableConfig = 0x00ff;
    static constexpr uint16_t kReplyFlag = 0x0100;

    // header, length, command, error, footer
    static constexpr uint8_t kReplyOverhead = 4 + 2 + 2 + 2 + 4;

    // the largest block that opening the port needs: the console output buffer and the reply frame
    static constexpr uint32_t kMinFreeBlock = 2048;

public:
    LD24xxSerial(uint32_t baud);

    // returns -1 if no byte is available
    int read();

    // receive errors: overruns and framing errors of the UART
    uint16_t getErrors() const;

    // discards the data received so far and sends a command frame, the data has kMaxCommandData bytes or less
    void sendCommand(uint16_t command, const uint8_t *data, uint8_t length);

    // collects a reply frame, returns true when one has been received
    bool readReply();

    // the last reply that readReply() returned
    uint16_t getReplyCommand() const;
    uint16_t getReplyError() const;
    const uint8_t *getReplyData() const;
    uint8_t getReplyDataLength() const;

    // bytes received since the last command
    uint16_t getReceivedBytes() const;

    static uint16_t getUint16(const uint8_t *ptr);
    static uint32_t getUint32(const uint8_t *ptr);
    static uint8_t *putUint16(uint8_t *ptr, uint16_t value);
    static uint8_t *putUint32(uint8_t *ptr, uint32_t value);

private:
    LD24xxHardwareUart _uart;
    uint8_t _frame[kMaxFrameLength];
    uint8_t _length;
    uint8_t _frameLength;
    uint16_t _rxBytes;
};

inline int LD24xxSerial::read()
{
    return _uart.read();
}

inline uint16_t LD24xxSerial::getErrors() const
{
    return _uart.getErrors();
}

inline uint16_t LD24xxSerial::getReplyCommand() const
{
    return getUint16(&_frame[6]);
}

inline uint16_t LD24xxSerial::getReplyError() const
{
    return getUint16(&_frame[8]);
}

inline const uint8_t *LD24xxSerial::getReplyData() const
{
    return &_frame[10];
}

inline uint8_t LD24xxSerial::getReplyDataLength() const
{
    return _frameLength - kReplyOverhead;
}

inline uint16_t LD24xxSerial::getReceivedBytes() const
{
    return _rxBytes;
}

inline uint16_t LD24xxSerial::getUint16(const uint8_t *ptr)
{
    return ptr[0] | (ptr[1] << 8);
}

inline uint32_t LD24xxSerial::getUint32(const uint8_t *ptr)
{
    return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
}

inline uint8_t *LD24xxSerial::putUint16(uint8_t *ptr, uint16_t value)
{
    *ptr++ = static_cast<uint8_t>(value);
    *ptr++ = static_cast<uint8_t>(value >> 8);
    return ptr;
}

inline uint8_t *LD24xxSerial::putUint32(uint8_t *ptr, uint32_t value)
{
    ptr = putUint16(ptr, static_cast<uint16_t>(value));
    return putUint16(ptr, static_cast<uint16_t>(value >> 16));
}

#endif
