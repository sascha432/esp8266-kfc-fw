/**
 * Author: sascha_lammers@gmx.de
 */

#include <SoftwareSerial.h>
#include <LoopFunctions.h>
#include <DumpBinary.h>
#include "serial_handler.h"
#include "Serial2TcpBase.h"
#include "Serial2TcpServer.h"
#include "Serial2TcpClient.h"
#include "s2t_nvt.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Serial2TcpBase *Serial2TcpBase::_instance = nullptr;
#if DEBUG
bool Serial2TcpBase::_debugOutput = false;
#endif

Serial2TcpBase::Serial2TcpBase(Stream &serial, const Serial2TCP::Serial2Tcp_t &config) : _serial(serial), _config(config)
{
}

Serial2TcpBase::~Serial2TcpBase()
{
    switch(_config.serial_port) {
        case Serial2TCP::SerialPortType::SOFTWARE:
            delete reinterpret_cast<SoftwareSerial *>(&_serial);
            // fall through
        case Serial2TCP::SerialPortType::SERIAL1:
            LoopFunctions::remove(Serial2TcpBase::handleSerialDataLoop);
            break;
        default:
        case Serial2TCP::SerialPortType::SERIAL0:
            SerialHandler::getInstance().removeHandler(onSerialData);
            break;
    }
}

void Serial2TcpBase::_setBaudRate(uint32_t baudrate)
{
    switch(_config.serial_port) {
        case Serial2TCP::SerialPortType::SOFTWARE: {
                auto &serial = *reinterpret_cast<SoftwareSerial *>(&_serial);
                if (serial.baudRate() != baudrate) {
                    serial.end();
                    serial.begin(baudrate);
                    DEBUGV("SoftwareSerial baudrate=%u\n", baudrate);
                }
            }
            break;
        case Serial2TCP::SerialPortType::SERIAL1:
            if (Serial1.baudRate() != (int)baudrate) {
                Serial1.end();
                Serial1.begin(baudrate);
                DEBUGV("Serial1 baudrate=%u\n", baudrate);
            }
            break;
        case Serial2TCP::SerialPortType::SERIAL0:
        default:
            if (Serial0.baudRate() != (int)baudrate) {
                Serial0.end();
                Serial0.begin(baudrate);
                DEBUGV("Serial baudrate=%u\n", baudrate);
            }
            break;
    }
}

Serial2TcpBase *Serial2TcpBase::createInstance(const Serial2TCP::Serial2Tcp_t &cfg, const char *hostname)
{
    destroyInstance();

    if (hostname == nullptr) {
        if (!Serial2TCP::isEnabled()) {
            return nullptr;
        }
    }

    Stream *serialPort;
    switch(cfg.serial_port) {
        case Serial2TCP::SerialPortType::SOFTWARE: {
                auto softwareSerial = new SoftwareSerial(cfg.rx_pin, cfg.tx_pin);
                softwareSerial->begin(cfg.baudrate);
                serialPort = softwareSerial;
                DEBUGV("SoftwareSerial: rx=%d tx=%d baud=%d\n", cfg.rx_pin, cfg.tx_pin, cfg.baudrate);
            }
            LoopFunctions::add(Serial2TcpBase::handleSerialDataLoop);
            break;
        case Serial2TCP::SerialPortType::SERIAL1:
            serialPort = &Serial1;
            Serial1.begin(cfg.baudrate);
            DEBUGV("Serial1 baud %u\n", cfg.baudrate);
            LoopFunctions::add(Serial2TcpBase::handleSerialDataLoop);
            break;
        default:
        case Serial2TCP::SerialPortType::SERIAL0:
            serialPort = &Serial0;
            if ((int)cfg.baudrate != Serial0.baudRate()) {
                Serial0.end();
                Serial0.begin(cfg.baudrate);
            }
            DEBUGV("Serial baud=%u\n", cfg.baudrate);
            SerialHandler::getInstance().addHandler(onSerialData, SerialHandler::ANY);
            // SerialHandler::getInstance().addHandler(onSerialData, SerialHandler::RECEIVE|SerialHandler::TRANSMIT);
            break;
    }

    if (cfg.mode == Serial2TCP::ModeType::SERVER) {
        _instance = new Serial2TcpServer(*serialPort, cfg);
        DEBUGV("server mode port %u\n", cfg.port);
    }
    else  {
        if (!hostname) {
            hostname = Serial2TCP::getHostname();
        }
        _instance = new Serial2TcpClient(*serialPort, hostname, cfg);
        DEBUGV("client mode: %s:%u\n", hostname, cfg.port);
    }
    if (cfg.authentication) {
        _instance->setAuthenticationParameters(Serial2TCP::getUsername(), Serial2TCP::getPassword());
    }
    return _instance;
}

void Serial2TcpBase::destroyInstance()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

// serial handlers

void Serial2TcpBase::onSerialData(uint8_t type, const uint8_t *buffer, size_t len)
{
    DEBUGV("type=%u len=%u inst=%p\n", type, len, _instance);
    if (_instance) {
        _instance->_onSerialData(type, buffer, len);
    }
}

void Serial2TcpBase::handleSerialDataLoop()
{
    if (_instance) {
        auto len = (unsigned int)_instance->_serial.available();
        DEBUGV("len=%u\n", len);
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

void Serial2TcpBase::_processData(Serial2TcpConnection *conn, const char *data, size_t len)
{
#if 0
    _serialWrite(conn, data, len);
#else
    auto client = conn->getClient();
    auto &buffer = conn->getNvtBuffer();
    auto ptr = data;
    auto count = len;

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
                    DEBUGV_NVT("NVT_WONT\n");
                    buffer.write(*ptr);
                    break;
                case NVT_WILL:
                    DEBUGV_NVT("NVT_WILL\n");
                    buffer.write(*ptr);
                    break;
                case NVT_SB:
                    DEBUGV_NVT("NVT_SB\n");
                    buffer.write(*ptr);
                    break;
                case NVT_SE:
                    DEBUGV_NVT("NVT_SE\n");
                    buffer.clear();
                    break;
                case NVT_NOP:
                    // DEBUGV_NVT("NVT_NOP\n");
                    buffer.clear();
                    break;
                case NVT_AYT: {
                        DEBUGV_NVT("NVT_AYT\n");
                        buffer.clear();
                        PrintString str;
                        str.printf_P(PSTR("KFCFW %s\n"), KFCFWConfiguration::getShortFirmwareVersion().c_str());
                        if (client) {
                            client->write(str.c_str(), str.length());
                        }
                    }
                    break;
                case NVT_IP:
                    DEBUGV_NVT("NVT_IP\n");
                    conn->close();
                    return;
                // case NVT_IAC:
                //     _serialWrite(conn, NVT_IAC);
                //     // fallthrough
                default:
                    DEBUGV_NVT("NVT error %u\n", (int)*ptr);
                    // output.write(NVT_IAC);
                    // output.write(*ptr);
                    _serialWrite(conn, NVT_IAC);
                    _serialWrite(conn, *ptr);
                    buffer.clear();
                    break;
            }
        }
        else if (*ptr == NVT_SB_CPO && buffer.length() == 2) {
            DEBUGV_NVT("NVT_SB_CPO\n");
            buffer.write(NVT_SB_CPO);
        }
        else if (buffer.length() > 2) {
            buffer.write(*ptr);
            if (*ptr == NVT_SE) {
                DEBUGV_NVT("NVT_SE\n");
                if (buffer.charAt(buffer.length() - 2) == NVT_IAC) { // end of sequence
                    auto data = buffer.getChar() + 2;
                    auto dataLen = buffer.length() - 4;
                    DEBUGV_NVT("NVT_IAC dataLen=%d\n", dataLen);
                    if (*data == NVT_SB_CPO) {
                        dataLen--;
                        data++;
                        DEBUGV_NVT("NVT_SB_CPO dataLen=%d\n", dataLen);
                        if (*data == NVT_CPO_SET_BAUDRATE) {
                            dataLen--;
                            DEBUGV_NVT("NVT_CPO_SET_BAUDRATE dataLen=%d\n", dataLen);
                            data++;
                            uint32_t baudRate = 0;
                            while(dataLen--) {
                                DEBUGV_NVT("dataLen=%d\n", dataLen);
                                if (*data == NVT_IAC) {
                                    DEBUGV_NVT("NVT_IAC\n");
                                    if (dataLen && *(data + 1) == NVT_IAC) { // skip double NVT_IAC
                                        DEBUGV_NVT("NVT_IAC\n");
                                        dataLen--;
                                        data++;
                                    }
                                }
                                baudRate <<= 8;
                                baudRate |= *data;
                                data++;
                            }
                            DEBUGV_NVT("NVT_CPO_SET_BAUDRATE %u\n", baudRate);
                            if (baudRate) {
                                _setBaudRate(baudRate);
                            }
                            if (client) {
                                char buf[] = { NVT_IAC, NVT_SB_CPO, NVT_WILL, NVT_CPO_SET_BAUDRATE, NVT_IAC };
                                // client->write(buf, sizeof(buf));
                            }
                        }
                        else if (*data == NVT_SB_BINARY) {
                            data++;
                            dataLen--;
                            if (*data == NVT_IAC) {
                                if (client) {
                                    char buf[] = { NVT_IAC, NVT_SB_CPO, NVT_WONT, NVT_SB_BINARY, NVT_IAC };
                                    // client->write(buf, sizeof(buf));
                                }
                            }
                        }
                        else {
                            data++;
                            while(dataLen--) {
                                DEBUGV_NVT("data %02x %d\n", *data, *data);
                                data++;
                            }

                        }
                    }
                    buffer.clear();
                }
            }
            if (buffer.length() > 32) {
                // output.write(buffer.getConstChar(), buffer.length());
                _serialWrite(conn, buffer.getConstChar(), buffer.length());
                buffer.clear();
            }
        }
        else {
            if (buffer.length()) {
                // output.write(buffer.getConstChar(), buffer.length());
                // output.write(*ptr);
                _serialWrite(conn, buffer.getConstChar(), buffer.length());
                buffer.clear();
                _serialWrite(conn, *ptr);
            }
            else {
                output.write(*ptr);
            }
        }
        ptr++;
    }
    if (output.length()) {
        _serialWrite(conn, output.getConstChar(), output.length());
    }
#endif
}

// serial output methods

size_t Serial2TcpBase::_serialWrite(Serial2TcpConnection *conn, const char *data, size_t len)
{
#if 1
    SerialHandler::getInstance().writeToReceive(SerialHandler::REMOTE_RX, reinterpret_cast<const uint8_t *>(data), len);
    return len;
#else
    auto written = _serial.write(data, len);
    if (written < len) {
        ::printf(PSTR("_serialWrite %u/%u\n"), written, len);
    }
    return written;
#endif
}

size_t Serial2TcpBase::_serialWrite(Serial2TcpConnection *conn, char data)
{
#if 1
    SerialHandler::getInstance().writeToReceive(SerialHandler::REMOTE_RX, reinterpret_cast<const uint8_t *>(&data), sizeof(data));
    return sizeof(data);
#else
    return _serial.write(data);
#endif
}

void Serial2TcpBase::end()
{
    if (_config.serial_port == Serial2TCP::SerialPortType::SERIAL0) {
        // reset to default baudrate
        if (_config.baudrate != KFC_SERIAL_RATE) {
            Serial0.end();
            Serial0.begin(KFC_SERIAL_RATE);
        }
    }
}

// tcp handlers

void Serial2TcpBase::handleError(void *arg, AsyncClient *client, int8_t error)
{
    DEBUGV("arg=%p client=%p error=%u\n", arg, client, error);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, PrintString(F("Error #%d"), error));
}

void Serial2TcpBase::handleConnect(void *arg, AsyncClient *client)
{
    DEBUGV("arg=%p client=%p\n", arg, client);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onConnect(client);
}

void Serial2TcpBase::handleData(void *arg, AsyncClient *client, void *data, size_t len)
{
#if 1
    DEBUGV("arg=%p client=%p len=%u\n", arg, client, len);
#else
    PrintString str;
    if (data && len) {
        DumpBinary d(str);
        d.setPerLine(128);
        d.dump(data, std::min((size_t)128, len));
        str.trim();
    }
    DEBUGV("arg=%p client=%p len=%u data=%s\n", arg, client, len, str.c_str());
#endif
    reinterpret_cast<Serial2TcpBase *>(arg)->_onData(client, data, len);
}

void Serial2TcpBase::handleDisconnect(void *arg, AsyncClient *client)
{
    DEBUGV("arg=%p client=%p\n", arg, client);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, F("Disconnect"));
}

void Serial2TcpBase::handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
    DEBUGV("arg=%p client=%p time=%u\n", arg, client, time);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, PrintString(F("Timeout %u"), time));
}

void Serial2TcpBase::handleAck(void *arg, AsyncClient *client, size_t len, uint32_t time)
{
    // DEBUGV("arg=%p client=%p len=%u time=%u\n", arg, client, len, time);
}

void Serial2TcpBase::handlePoll(void *arg, AsyncClient *client)
{
    // DEBUGV("arg=%p client=%p\n", arg, client);
}

void Serial2TcpBase::_onSerialData(uint8_t type, const uint8_t *buffer, size_t len)
{
    // DEBUGV("type=%d len=%u\n", type, len);
}

void Serial2TcpBase::_onConnect(AsyncClient *client)
{
    // DEBUGV("_onConnect client=%p\n", client);
}

void Serial2TcpBase::_onData(AsyncClient *client, void *data, size_t len)
{
    // DEBUGV("_onData client=%p len=%u\n", client, len);
}

void Serial2TcpBase::_onDisconnect(AsyncClient *client, const String &reason)
{
    // DEBUGV("_onDisconnect client=%p reason=%s\n", client, reason.c_str());
}
