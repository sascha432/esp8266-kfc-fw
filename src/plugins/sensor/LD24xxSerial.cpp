/**
 * Author: sascha_lammers@gmx.de
 */

#include "LD24xxSerial.h"

#if IOT_SENSOR_HAVE_LD24XX

#include <algorithm>
#include "serial_handler.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if KFC_TWOWIRE_SDA == 2 || KFC_TWOWIRE_SCL == 2
#    error GPIO2 sends to the LD24xx sensor (UART1 TX), I2C needs other pins (KFC_TWOWIRE_SDA, KFC_TWOWIRE_SCL)
#endif

namespace {

    constexpr uint8_t kFrameHeader[4] = { 0xfd, 0xfc, 0xfb, 0xfa };
    constexpr uint8_t kFrameFooter[4] = { 0x04, 0x03, 0x02, 0x01 };

}

LD24xxConsoleHold::LD24xxConsoleHold() :
    _dropped(0)
{
}

int LD24xxConsoleHold::available()
{
    return 0;
}

int LD24xxConsoleHold::read()
{
    return -1;
}

int LD24xxConsoleHold::peek()
{
    return -1;
}

size_t LD24xxConsoleHold::write(uint8_t data)
{
    return write(&data, sizeof(data));
}

size_t LD24xxConsoleHold::write(const uint8_t *buffer, size_t size)
{
    size_t room = LD24xx::kConsoleHoldSize - std::min<size_t>(_buffer.length(), LD24xx::kConsoleHoldSize);
    size_t length = std::min(size, room);
    if (length) {
        _buffer.concat(reinterpret_cast<const char *>(buffer), length);
    }
    _dropped += size - length;
    // the caller does not have to retry
    return size;
}

void LD24xxConsoleHold::release(Print &output)
{
    if (_buffer.length()) {
        output.write(reinterpret_cast<const uint8_t *>(_buffer.c_str()), _buffer.length());
        _buffer = String();
    }
    if (_dropped) {
        output.printf_P(PSTR("%u bytes of console output dropped while the radar sensor used UART0\n"), static_cast<unsigned>(_dropped));
        _dropped = 0;
    }
}

LD24xxHardwareUart::LD24xxHardwareUart(uint32_t baud) :
    _hold(),
    _input(nullptr),
    _consoleBaud(Serial0.baudRate()),
    _baud(baud),
    _errors(0),
    _holding(false)
{
    // UART1 keeps GPIO2 at the idle level between the commands, the sensor's RX must not float
    if (!Serial1) {
        Serial1.begin(baud, SERIAL_8N1, SERIAL_TX_ONLY);
    }
    else {
        Serial1.updateBaudRate(baud);
    }
    _pauseConsole();
    // the console output written before the pause leaves on GPIO1 before the TX signal is swapped away
    Serial0.flush();
    if (_baud != _consoleBaud) {
        Serial0.updateBaudRate(_baud);
    }
    // GPIO1 stays high for the USB serial chip, the swapped TX signal goes to GPIO15, which is not set to the UART
    // function and stays an input. GPIO3 keeps the UART function: a pad without it reads low, the UART would see a
    // start bit until the swap. With the swap it is the unused CTS input
    GPOS = (1U << 1);
    pinMode(1, OUTPUT);
    pinMode(LD24xx::kRxPin, FUNCTION_4);
    IOSWAP |= (1U << IOSWAPU0);
    // console input that arrived before the swap
    while (Serial0.read() >= 0) {
    }
}

LD24xxHardwareUart::~LD24xxHardwareUart()
{
    IOSWAP &= ~(1U << IOSWAPU0);
    pinMode(LD24xx::kRxPin, INPUT);
    pinMode(1, SPECIAL);
    // the sensor reports all the time outside the configuration mode: a byte that it was sending during the switch
    // ends with the idle level of GPIO3 and arrives as garbage within one byte, wait for two before the rest of what
    // the sensor sent is discarded
    delayMicroseconds(20 * 1000000U / _baud + 1);
    while (Serial0.read() >= 0) {
    }
    if (_baud != _consoleBaud) {
        Serial0.updateBaudRate(_consoleBaud);
    }
    _resumeConsole();
}

int LD24xxHardwareUart::read()
{
    auto data = Serial0.read();
    if (Serial0.hasOverrun() || Serial0.hasRxError()) {
        _errors++;
    }
    return data;
}

void LD24xxHardwareUart::write(const uint8_t *data, size_t length)
{
    Serial1.write(data, length);
}

void LD24xxHardwareUart::_pauseConsole()
{
    // the main loop is the only writer of the console streams
    auto &streams = *serialHandler.getStreams();
    auto iterator = std::find(streams.begin(), streams.end(), static_cast<Stream *>(&Serial0));
    if (iterator != streams.end()) {
        *iterator = &_hold;
        _holding = true;
    }
    if (serialHandler.getInput() == &Serial0) {
        _input = &Serial0;
        serialHandler.setInput(&NullSerial);
    }
}

void LD24xxHardwareUart::_resumeConsole()
{
    if (_input) {
        serialHandler.setInput(_input);
    }
    if (_holding) {
        auto &streams = *serialHandler.getStreams();
        auto iterator = std::find(streams.begin(), streams.end(), static_cast<Stream *>(&_hold));
        if (iterator != streams.end()) {
            *iterator = &Serial0;
        }
        _hold.release(Serial0);
    }
}

LD24xxSerial::LD24xxSerial(uint32_t baud) :
    _uart(baud),
    _frame(),
    _length(0),
    _frameLength(0),
    _rxBytes(0)
{
}

void LD24xxSerial::sendCommand(uint16_t command, const uint8_t *data, uint8_t length)
{
    uint8_t frame[sizeof(kFrameHeader) + 2 + 2 + kMaxCommandData + sizeof(kFrameFooter)];
    if (length > kMaxCommandData) {
        __LDBG_printf("command=%04x data length=%u too long", command, length);
        return;
    }
    auto ptr = frame;
    memcpy(ptr, kFrameHeader, sizeof(kFrameHeader));
    ptr += sizeof(kFrameHeader);
    ptr = putUint16(ptr, length + 2);
    ptr = putUint16(ptr, command);
    if (length) {
        memcpy(ptr, data, length);
        ptr += length;
    }
    memcpy(ptr, kFrameFooter, sizeof(kFrameFooter));
    ptr += sizeof(kFrameFooter);

    // discard everything the sensor reported before the command
    while (read() >= 0) {
    }
    _length = 0;
    _rxBytes = 0;
    _uart.write(frame, ptr - frame);
}

bool LD24xxSerial::readReply()
{
    int data;
    while ((data = read()) >= 0) {
        auto byte = static_cast<uint8_t>(data);
        _rxBytes++;
        // synchronize to the header, a mismatch restarts the frame
        if (_length < sizeof(kFrameHeader)) {
            if (byte != kFrameHeader[_length]) {
                _length = 0;
                if (byte != kFrameHeader[0]) {
                    continue;
                }
            }
            _frame[_length++] = byte;
            continue;
        }
        _frame[_length++] = byte;
        if (_length < 6) {
            continue;
        }
        auto frameLength = getUint16(&_frame[4]) + 10U;
        if (frameLength < kReplyOverhead || frameLength > sizeof(_frame)) {
            __LDBG_printf("invalid frame length=%u", frameLength);
            _length = 0;
            continue;
        }
        if (_length == frameLength) {
            _length = 0;
            if (memcmp(&_frame[frameLength - sizeof(kFrameFooter)], kFrameFooter, sizeof(kFrameFooter)) == 0) {
                _frameLength = frameLength;
                return true;
            }
            __LDBG_printf("invalid frame footer");
        }
    }
    return false;
}

#endif
