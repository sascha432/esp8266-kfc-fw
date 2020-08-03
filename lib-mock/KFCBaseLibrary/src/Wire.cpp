/**
* Author: sascha_lammers@gmx.de
*/

#if _WIN32

#include "Wire.h"
#include "PrintString.h"

#define ENABLE_TWO_WIRE   0

#ifndef DEVICE_NAME
#define DEVICE_NAME "COM4"
#endif

#define DEBUG_SERIAL_ECHO 1

static HANDLE hComm = 0;

HANDLE get_com_handle() {
#if ENABLE_TWO_WIRE
    if (hComm) {
        return hComm;
    }
    hComm = CreateFileA("\\\\.\\" DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hComm == INVALID_HANDLE_VALUE) {
        __debugbreak_and_panic_printf_P(PSTR("Cannot open " DEVICE_NAME "\n"));
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    SetCommState(hComm, &dcbSerialParams);

    COMMTIMEOUTS timeouts = { MAXDWORD, 0, 0, 50, 10 };
    if (!SetCommTimeouts(hComm, &timeouts)) {
        __debugbreak_and_panic_printf_P(PSTR("Cannot open " DEVICE_NAME "\n"));
    }

    if (!SetCommMask(hComm, EV_RXCHAR)) {
        __debugbreak_and_panic_printf_P(PSTR("Cannot open " DEVICE_NAME "\n"));
    }
    return hComm;
#else
    return 0;
#endif
}

static String line;

String serial_read_byte(HANDLE hComm) {
    DWORD read;
    char data;
    if (ReadFile(hComm, &data, sizeof(data), &read, NULL) && read) {
        if (data == '\n') {
            if (line.length()) {
                auto tmp = line;
                line = String();
#if DEBUG_SERIAL_ECHO
                Serial.print("serial_read: ");
                Serial.println(tmp);
#endif
                return tmp;
            }
        }
        else if (data == '\r') {
        }
        else {
            line += data;
        }
    }
    return String();
}



int serial_write(HANDLE hComm, const char *buffer, DWORD length) {
    DWORD written = 0;
    if (!WriteFile(hComm, buffer, length, &written, NULL)) {
        __debugbreak_and_panic_printf_P(PSTR(DEVICE_NAME " write error"));
    }
#if DEBUG_SERIAL_ECHO
    Serial.print(F("serial_write: "));
    Serial.print(buffer);
#endif
    return written;
}

void (*TwoWire::user_onRequest)(void);
void (*TwoWire::user_onReceive)(int);

TwoWire::TwoWire() : _hComm(get_com_handle())
{
}

void TwoWire::begin(void)
{
}

void TwoWire::begin(uint8_t address)
{
  begin();
}

void TwoWire::begin(int address)
{
  begin((uint8_t)address);
}

void TwoWire::end(void)
{
}

void TwoWire::setClock(uint32_t clock)
{
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop)
{
    _buffer = F("+I2CR=");
    write(address);
    write(quantity);
    _buffer += '\n';
    _out = BufferStream();
    if (serial_write(_hComm, _buffer.c_str(), _buffer.length()) == _buffer.length()) {
        return _waitForResponse();
    }
    return 0;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop) {
    return requestFrom((uint8_t)address, (uint8_t)quantity, (uint32_t)0, (uint8_t)0, (uint8_t)sendStop);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity)
{
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)true);
}

uint8_t TwoWire::requestFrom(int address, int quantity)
{
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)true);
}

uint8_t TwoWire::requestFrom(int address, int quantity, int sendStop)
{
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)sendStop);
}

void TwoWire::beginTransmission(uint8_t address)
{
    _buffer = "+I2CT=";
    write(address);
}

void TwoWire::beginTransmission(int address)
{
  beginTransmission((uint8_t)address);
}

uint8_t TwoWire::endTransmission(uint8_t sendStop)
{
    _buffer += '\n';
    if (serial_write(_hComm, _buffer.c_str(), _buffer.length()) != _buffer.length()) {
        return 1;
    }
    return 0;
}

uint8_t TwoWire::endTransmission(void)
{
    return endTransmission(true);
}

size_t TwoWire::write(uint8_t data)
{
    _buffer += PrintString(F("%02x"), data & 0xff);
    return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    size_t written = 0;
    while (quantity--) {
        written += write(*data++);
    }
    return written;
}

int TwoWire::available(void)
{
    return _out.available();
}

int TwoWire::_waitForResponse() {
    int i = 1000;
    String in;
    while (in.length() == 0 && i-- > 0) {
        in = serial_read_byte(_hComm);
        in.toUpperCase();
        if (in.length() && in.startsWith(F("+I2CT="))) {
            auto ptr = in.c_str() + 6;
            size_t len = in.length() - 6;
            while (len-- && len--) {
                char buf[3] = { *ptr, *(ptr + 1), 0 };
                ptr += 2;
                _out += (uint8_t)strtoul(buf, nullptr, 16);
            }
            break;
        }
        else {
            in = String();
        }
        Sleep(1);
    }
    return available();
}

int TwoWire::read(void)
{
    return _out.read();
}

int TwoWire::peek(void)
{
    return _out.peek();
}

void TwoWire::flush(void)
{
}

// behind the scenes function that is called when data is received
void TwoWire::onReceiveService(uint8_t* inBytes, int numBytes)
{
  //user_onReceive(numBytes);
}

// behind the scenes function that is called when data is requested
void TwoWire::onRequestService(void)
{
  //// don't bother if user hasn't registered a callback
  //if(!user_onRequest){
  //  return;
  //}
  //user_onRequest();
}

// sets function called on slave write
void TwoWire::onReceive( void (*function)(int) )
{
  user_onReceive = function;
}

// sets function called on slave read
void TwoWire::onRequest( void (*function)(void) )
{
  user_onRequest = function;
}

TwoWire Wire;

#endif
