/**
 * Author: sascha_lammers@gmx.de
 */

#if SERIAL2TCP

#include <SoftwareSerial.h>
#include <LoopFunctions.h>
#include "serial_handler.h"
#include "Serial2TcpBase.h"
#include "Serial2TcpServer.h"
#include "Serial2TcpClient.h"

#if IOT_DIMMER_MODULE
#include "plugins/dimmer_module/dimmer_module.h"
#endif

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Serial2TcpBase *Serial2TcpBase::_instance = nullptr;
#if IOT_DIMMER_MODULE
bool Serial2TcpBase::_resetAtmega = true;
#endif
#if DEBUG
bool Serial2TcpBase::_debugOutput = false;
#endif

Serial2TcpBase::Serial2TcpBase(Stream &serial, uint8_t serialPort) : _serial(serial), _serialPort(serialPort), _hasAuthentication(false), _idleTimeout(300), _keepAlive(0), _port(2323) {
}

Serial2TcpBase::~Serial2TcpBase() {
    switch(_serialPort) {
        case SERIAL2TCP_SOFTWARE_CUSTOM:
            delete reinterpret_cast<SoftwareSerial *>(&_serial);
            // fall through
        case SERIAL2TCP_HARDWARE_SERIAL1:
            LoopFunctions::remove(Serial2TcpBase::handleSerialDataLoop);
            break;
        default:
        case SERIAL2TCP_HARDWARE_SERIAL:
            SerialHandler::getInstance().removeHandler(onSerialData);
            break;
    }
}

Serial2TcpBase *Serial2TcpBase::createInstance() {

    destroyInstance();

    auto flags = config._H_GET(Config().flags);
    if (flags.serial2TCPMode == SERIAL2TCP_MODE_DISABLED) {
        return nullptr;
    }

    auto cfg = config._H_GET(Config().serial2tcp);
    Stream *serialPort;
    switch(cfg.serial_port) {
        case SERIAL2TCP_SOFTWARE_CUSTOM: {
                _debug_printf_P(PSTR("Serial2TcpBase::createInstance() software serial: RX pin %d, TX pin %d, baud %d\n"), cfg.rx_pin, cfg.tx_pin, cfg.baud_rate)
                auto softwareSerial = new SoftwareSerial(cfg.rx_pin, cfg.tx_pin);
                softwareSerial->begin(cfg.baud_rate);
                serialPort = softwareSerial;
            }
            LoopFunctions::add(Serial2TcpBase::handleSerialDataLoop);
            break;
        case SERIAL2TCP_HARDWARE_SERIAL1:
            _debug_printf_P(PSTR("Serial2TcpBase::createInstance(): hardware port: Serial1 baud %u\n"), cfg.baud_rate)
            serialPort = &Serial1;
            Serial1.begin(cfg.baud_rate);
            LoopFunctions::add(Serial2TcpBase::handleSerialDataLoop);
            break;
        default:
        case SERIAL2TCP_HARDWARE_SERIAL:
            _debug_printf_P(PSTR("Serial2TcpBase::createInstance(): hardware port: Serial baud %u\n"), cfg.baud_rate)
            serialPort = &Serial;
            Serial.begin(cfg.baud_rate);
            SerialHandler::getInstance().addHandler(onSerialData, SerialHandler::RECEIVE|SerialHandler::LOCAL_TX);
            break;
    }

    if (isServer(flags)) {
        _debug_printf_P(PSTR("Serial2TcpBase::createInstance(): server mode: port %u\n"), cfg.port);
        _instance = new Serial2TcpServer(*serialPort, cfg.serial_port);
    }
    else  {
        _debug_printf_P(PSTR("Serial2TcpBase::createInstance(): client mode: %s:%u\n"), cfg.host, cfg.port);
        _instance = new Serial2TcpClient(*serialPort, cfg.serial_port);
    }
    _instance->setAuthentication(cfg.auth_mode);
    if (cfg.auth_mode) {
        _instance->setAuthenticationParameters(nullptr, cfg.password);
    }
    _instance->setConnectionParameters(cfg.host, cfg.port, cfg.auto_connect, cfg.auto_reconnect);

    return _instance;
}

void Serial2TcpBase::destroyInstance() {
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

void Serial2TcpBase::_setBaudRate(uint32_t rate) {
    _debug_printf_P(PSTR("Serial2TcpBase::_setBaudRate(%u)\n"), rate);
    switch(_serialPort) {
        case SERIAL2TCP_SOFTWARE_CUSTOM:
            reinterpret_cast<SoftwareSerial *>(&_serial)->begin(rate);
            break;
        case SERIAL2TCP_HARDWARE_SERIAL1:
        default:
        case SERIAL2TCP_HARDWARE_SERIAL:
            reinterpret_cast<HardwareSerial *>(&_serial)->begin(rate);
            break;
    }
}

void Serial2TcpBase::setConnectionParameters(const char *host, uint16_t port, bool autoConnect, uint8_t autoReconnect) {
    _host = host;
    _port = port;
    _autoConnect = autoConnect;
    _autoReconnect = autoReconnect;
}

// serial handlers

void Serial2TcpBase::onSerialData(uint8_t type, const uint8_t *buffer, size_t len) {
    if (_instance) {
        _instance->_onSerialData(type, buffer, len);
    }
}

void Serial2TcpBase::handleSerialDataLoop() {
    if (_instance) {
        auto len = (unsigned int)_instance->_serial.available();
        if (len) {
            uint8_t buffer[128];
            if (len >= sizeof(buffer)) {
                len = sizeof(buffer) - 1;
            }
            len = _instance->_serial.readBytes(buffer, len);
            _instance->_onSerialData(SerialHandler::RECEIVE, buffer, len);
        }
    }
}

// network virtual terminal handler

void Serial2TcpBase::_processData(Serial2TcpConnection *conn, const char *data, size_t len) {
#if 1
#if IOT_DIMMER_MODULE
    if (_resetAtmega) {
        _resetAtmega = false;
        Scheduler.addTimer(100, false, [](EventScheduler::TimerPtr timer) {
            DimmerModulePlugin::resetDimmerMCU();
        });
    }
#endif
    _getSerial().write(data, len);
    // Serial2TcpBase::_serialWrite(conn, data, len);
#else
    auto &buffer = conn->getNvtBuffer();
    auto ptr = data;
    auto count = len;

    // PrintString str;
    // str.print("> ");
    // while(count--) {
    //     str.printf_P(PSTR("%02x "), *ptr++);
    // }
    // Logger_warning(str);
    // ptr = data;
    // count = len;

    Buffer output;

    while(count--) {
        if (*ptr == NVT_IAC && buffer.length() == 0) {
            buffer.write(NVT_IAC);
        }
        else if (buffer.length() == 1) {
            switch(*ptr) {
                // case NVT_DONT:
                // case NVT_DO:
                // case NVT_GA:
                // case NVT_EL:
                // case NVT_EC:
                // case NVT_AO:
                // case NVT_BRK:
                // case NVT_DM:

                case NVT_WONT:
                case NVT_WILL:
                case NVT_SB:
                    buffer.write(*ptr);
                    break;
                case NVT_SE:
                case NVT_NOP:
                    buffer.clear();
                    break;
                case NVT_AYT: {
                        buffer.clear();
                        PrintString str;
                        str.printf_P(PSTR("KFCFW %s\n"), KFCFWConfiguration::getFirmwareVersion().c_str());
                        conn->getClient()->write(str.c_str(), str.length());
                    }
                    break;
                case NVT_IP:
                    conn->getClient()->close();
                    return;
                // case NVT_IAC:
                //     _serialWrite(conn, NVT_IAC);
                //     // fallthrough
                default:
                    output.write(NVT_IAC);
                    output.write(*ptr);
                    // _serialWrite(conn, NVT_IAC);
                    // _serialWrite(conn, *ptr);
                    buffer.clear();
                    break;
            }
        }
        else if (*ptr == NVT_SB_CPO && buffer.length() == 2) {
            buffer.write(NVT_SB_CPO);
        }
        else if (buffer.length() > 2) {
            buffer.write(*ptr);
            if (*ptr == NVT_SE) {
                if (buffer.charAt(buffer.length() - 2) == NVT_IAC) { // end of sequence
                    auto data = buffer.getChar() + 2;
                    auto dataLen = buffer.length() - 4;
                    if (*data == NVT_SB_CPO) {
                        dataLen--;
                        data++;
#if IOT_DIMMER_MODULE
                        if (*data == NVT_CPO_SET_CONTROL) {
                            dataLen--;
                            data++;
                            if (dataLen && *data & 0x02) {
                                // DimmerModulePlugin::resetDimmerMCU();
                                Scheduler.addTimer(100, false, [](EventScheduler::TimerPtr timer) {
                                    DimmerModulePlugin::resetDimmerMCU();
                                });
                            }
                        }
                        else
#endif
                        if (*data == NVT_CPO_SET_BAUDRATE) {
                            dataLen--;
                            data++;
                            uint32_t baudRate = 0;
                            while(dataLen--) {
                                if (*data == NVT_IAC) {
                                    if (dataLen && *(data + 1) == NVT_IAC) { // skip double NVT_IAC
                                        dataLen--;
                                        data++;
                                    }
                                }
                                baudRate <<= 8;
                                baudRate |= *data;
                                data++;
                            }
                            if (baudRate) {
                                _setBaudRate(baudRate);
                            }
                        }
                    }
                    buffer.clear();
                }
            }
            if (buffer.length() > 32) {
                output.write(buffer.getConstChar(), buffer.length());
                // _serialWrite(conn, buffer.getConstChar(), buffer.length());
                buffer.clear();
            }
        }
        else {
            if (buffer.length()) {
                output.write(buffer.getConstChar(), buffer.length());
                output.write(*ptr);
                // buffer.write(*ptr);
                // _serialWrite(conn, buffer.getConstChar(), buffer.length());
                buffer.clear();
            }
            else {
                output.write(*ptr);
            }
        }
        ptr++;
    }
    if (output.length()) {
        _serialWrite(conn, output.getChar(), output.length());
    }
#endif
}

// serial output methods

size_t Serial2TcpBase::_serialWrite(Serial2TcpConnection *conn, const char *data, size_t len) {
    auto written = _serial.write(data, len);

    // PrintString str;
    // auto count = len;
    // auto ptr = data;
    // str.print("+ ");
    // while(count--) {
    //     str.printf_P(PSTR("%02x "), *ptr++);
    // }
    // Logger_warning(str);


    if (written < len) {
        // TODO if writing to serial port fails, store data in "conn" tx buffer. conn might be nullptr
    }
    return written;
}

size_t Serial2TcpBase::_serialWrite(Serial2TcpConnection *conn, char data) {
    return _serialWrite(conn, &data, sizeof(data));
}

size_t Serial2TcpBase::_serialPrintf_P(Serial2TcpConnection *conn, PGM_P format, ...) {
    va_list arg;
    va_start(arg, format);
    char temp[128];
    char *buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1) {
        buffer = new char[len + 1];
        if (!buffer) {
            return 0;
        }
        va_start(arg, format);
        vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    auto written = _serialWrite(conn, buffer, len);
    if (buffer != temp) {
        delete[] buffer;
    }
    return written;
}


// tcp handlers

void Serial2TcpBase::handleError(void *arg, AsyncClient *client, int8_t error) {
    _debug_printf_P(PSTR("Serial2TcpBase::handleError(): error %u\n"), error);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, F("error"));
}

void Serial2TcpBase::handleConnect(void *arg, AsyncClient *client) {
    _debug_printf_P(PSTR("Serial2TcpBase::handleConnect()\n"));
    reinterpret_cast<Serial2TcpBase *>(arg)->_onConnect(client);
}

void Serial2TcpBase::handleData(void *arg, AsyncClient *client, void *data, size_t len) {
    _debug_printf_P(PSTR("Serial2TcpBase::handleData(): len %u\n"), len);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onData(client, data, len);
}

void Serial2TcpBase::handleDisconnect(void *arg, AsyncClient *client) {
    _debug_printf_P(PSTR("Serial2TcpBase::handleDisconnect()\n"));
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, F("disconnect"));
}

void Serial2TcpBase::handleTimeOut(void *arg, AsyncClient *client, uint32_t time) {
    _debug_printf_P(PSTR("Serial2TcpBase::handleTimeOut(): time %u\n"), time);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, F("timeout"));
}

void Serial2TcpBase::handleAck(void *arg, AsyncClient *client, size_t len, uint32_t time) {
    _debug_printf_P(PSTR("Serial2TcpBase::handleAck(): len %u, time %u\n"), len, time);
}

void Serial2TcpBase::handlePoll(void *arg, AsyncClient *client) {
    _debug_printf_P(PSTR("Serial2TcpBase::handlePoll()\n"));
}

void Serial2TcpBase::_onSerialData(uint8_t type, const uint8_t *buffer, size_t len) {
    _debug_printf_P(PSTR("Serial2TcpBase::_onData(): type %d, length %u\n"), type, len);
}

void Serial2TcpBase::_onConnect(AsyncClient *client) {
    _debug_printf_P(PSTR("Serial2TcpBase::_onConnect()\n"));
}

void Serial2TcpBase::_onData(AsyncClient *client, void *data, size_t len) {
    _debug_printf_P(PSTR("Serial2TcpBase::_onData(): length %u\n"), len);
}

void Serial2TcpBase::_onDisconnect(AsyncClient *client, const __FlashStringHelper *reason) {
    _debug_printf_P(PSTR("Serial2TcpBase::_onDisconnect(): reason: %s\n"), RFPSTR(reason));
}

#endif
