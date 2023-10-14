/**
 * Author: sascha_lammers@gmx.de
 */

#include <SoftwareSerial.h>
#include <LoopFunctions.h>
#include <DumpBinary.h>
#include <serial_handler.h>
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

Serial2TcpBase::Serial2TcpBase(Stream &serial, const Serial2TCP::Serial2Tcp_t &config) : _client(nullptr), _serial(serial), _config(config)
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
            if (_client) {
                serialHandler.removeClient(*_client);
            }
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
                    __DBGS2T("SoftwareSerial baudrate=%u\n", baudrate);
                }
            }
            break;
        case Serial2TCP::SerialPortType::SERIAL1:
            if (Serial1.baudRate() != (int)baudrate) {
                Serial1.end();
                Serial1.begin(baudrate);
                __DBGS2T("Serial1 baudrate=%u\n", baudrate);
            }
            break;
        case Serial2TCP::SerialPortType::SERIAL0:
        default:
            if (Serial0.baudRate() != (int)baudrate) {
                Serial0.end();
                Serial0.begin(baudrate);
                __DBGS2T("Serial baudrate=%u\n", baudrate);
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
    SerialHandler::Client *client = nullptr;
    switch(cfg.serial_port) {
        case Serial2TCP::SerialPortType::SOFTWARE: {
                auto softwareSerial = new SoftwareSerial(cfg.rx_pin, cfg.tx_pin);
                softwareSerial->begin(cfg.baudrate);
                serialPort = softwareSerial;
                __DBGS2T("SoftwareSerial: rx=%d tx=%d baud=%d\n", cfg.rx_pin, cfg.tx_pin, cfg.baudrate);
            }
            LOOP_FUNCTION_ADD(Serial2TcpBase::handleSerialDataLoop);
            break;
        case Serial2TCP::SerialPortType::SERIAL1:
            serialPort = &Serial1;
            Serial1.begin(cfg.baudrate);
            __DBGS2T("Serial1 baud %u\n", cfg.baudrate);
            LOOP_FUNCTION_ADD(Serial2TcpBase::handleSerialDataLoop);
            break;
        default:
        case Serial2TCP::SerialPortType::SERIAL0:
            serialPort = &Serial0;
            if ((int)cfg.baudrate != Serial0.baudRate()) {
                Serial0.end();
                Serial0.begin(cfg.baudrate);
            }
            __DBGS2T("Serial baud=%u\n", cfg.baudrate);
            client = &serialHandler.addClient(onSerialData, SerialHandler::EventType::NONE);
            break;
    }

    if (cfg.mode == Serial2TCP::ModeType::SERVER) {
        _instance = new Serial2TcpServer(*serialPort, cfg);
        __DBGS2T("server mode port %u\n", cfg.port);
    }
    else  {
        if (!hostname) {
            hostname = Serial2TCP::getHostname();
        }
        _instance = new Serial2TcpClient(*serialPort, hostname, cfg);
        __DBGS2T("client mode: %s:%u\n", hostname, cfg.port);
    }
    _instance->_setClient(client);
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

void Serial2TcpBase::onSerialData(Stream &client)
{
    //__DBGS2T("client %p available=%u\n", &client, client.available());
    if (_instance) {
        _instance->_onSerialData(client);
    }
}

void Serial2TcpBase::handleSerialDataLoop()
{
    if (_instance) {
        _instance->_onSerialData(_instance->_serial);
    }
}

// network virtual terminal handler

void Serial2TcpBase::_processTcpData(Serial2TcpConnection *conn, const char *data, size_t len)
{
    auto client = conn->getClient();
    auto &buffer = conn->getNvtBuffer();
    auto &stream = getStream();
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
                    __DBGS2T_NVT("NVT_WONT\n");
                    buffer.write(*ptr);
                    break;
                case NVT_WILL:
                    __DBGS2T_NVT("NVT_WILL\n");
                    buffer.write(*ptr);
                    break;
                case NVT_SB:
                    __DBGS2T_NVT("NVT_SB\n");
                    buffer.write(*ptr);
                    break;
                case NVT_SE:
                    __DBGS2T_NVT("NVT_SE\n");
                    buffer.clear();
                    break;
                case NVT_NOP:
                    // __DBGS2T_NVT("NVT_NOP\n");
                    buffer.clear();
                    break;
                case NVT_AYT: {
                        __DBGS2T_NVT("NVT_AYT\n");
                        buffer.clear();
                        PrintString str;
                        str.printf_P(PSTR("KFCFW %s\n"), (PGM_P)KFCFWConfiguration::getShortFirmwareVersion());
                        if (client) {
                            client->write(str.c_str(), str.length());
                        }
                    }
                    break;
                case NVT_IP:
                    __DBGS2T_NVT("NVT_IP\n");
                    conn->close();
                    return;
                // case NVT_IAC:
                //     _serialWrite(conn, NVT_IAC);
                //     // fallthrough
                default:
                    __DBGS2T_NVT("NVT error %u\n", (int)*ptr);
                    // output.write(NVT_IAC);
                    // output.write(*ptr);
                    stream.write(NVT_IAC);
                    stream.write(*ptr);
                    buffer.clear();
                    break;
            }
        }
        else if (*ptr == NVT_SB_CPO && buffer.length() == 2) {
            __DBGS2T_NVT("NVT_SB_CPO\n");
            buffer.write(NVT_SB_CPO);
        }
        else if (buffer.length() > 2) {
            buffer.write(*ptr);
            if (*ptr == NVT_SE) {
                __DBGS2T_NVT("NVT_SE\n");
                if (buffer.charAt(buffer.length() - 2) == NVT_IAC) { // end of sequence
                    auto data = buffer.getChar() + 2;
                    auto dataLen = buffer.length() - 4;
                    __DBGS2T_NVT("NVT_IAC dataLen=%d\n", dataLen);
                    if (*data == NVT_SB_CPO) {
                        dataLen--;
                        data++;
                        __DBGS2T_NVT("NVT_SB_CPO dataLen=%d\n", dataLen);
                        if (*data == NVT_CPO_SET_BAUDRATE) {
                            dataLen--;
                            __DBGS2T_NVT("NVT_CPO_SET_BAUDRATE dataLen=%d\n", dataLen);
                            data++;
                            uint32_t baudRate = 0;
                            while(dataLen--) {
                                __DBGS2T_NVT("dataLen=%d\n", dataLen);
                                if (*data == NVT_IAC) {
                                    __DBGS2T_NVT("NVT_IAC\n");
                                    if (dataLen && *(data + 1) == NVT_IAC) { // skip double NVT_IAC
                                        __DBGS2T_NVT("NVT_IAC\n");
                                        dataLen--;
                                        data++;
                                    }
                                }
                                baudRate <<= 8;
                                baudRate |= *data;
                                data++;
                            }
                            __DBGS2T_NVT("NVT_CPO_SET_BAUDRATE %u\n", baudRate);
                            if (baudRate) {
                                _setBaudRate(baudRate);
                            }
                            if (client) {
                                // char buf[] = { NVT_IAC, NVT_SB_CPO, NVT_WILL, NVT_CPO_SET_BAUDRATE, NVT_IAC };
                                // client->write(buf, sizeof(buf));
                            }
                        }
                        else if (*data == NVT_SB_BINARY) {
                            data++;
                            dataLen--;
                            if (*data == NVT_IAC) {
                                if (client) {
                                    // char buf[] = { NVT_IAC, NVT_SB_CPO, NVT_WONT, NVT_SB_BINARY, NVT_IAC };
                                    // client->write(buf, sizeof(buf));
                                }
                            }
                        }
                        else {
                            data++;
                            while(dataLen--) {
                                __DBGS2T_NVT("data %02x %d\n", *data, *data);
                                data++;
                            }

                        }
                    }
                    buffer.clear();
                }
            }
            if (buffer.length() > 32) {
                // output.write(buffer.getConstChar(), buffer.length());
                stream.write(buffer.get(), buffer.length());
                buffer.clear();
            }
        }
        else {
            if (buffer.length()) {
                // output.write(buffer.getConstChar(), buffer.length());
                // output.write(*ptr);
                stream.write(buffer.get(), buffer.length());
                buffer.clear();
                stream.write(*ptr);
            }
            else {
                output.write(*ptr);
            }
        }
        ptr++;
    }
    if (output.length()) {
        stream.write(output.get(), output.length());
    }
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
    __DBGS2T("arg=%p client=%p error=%u\n", arg, client, error);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, PrintString(F("Error #%d"), error));
}

void Serial2TcpBase::handleConnect(void *arg, AsyncClient *client)
{
    __DBGS2T("arg=%p client=%p\n", arg, client);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onConnect(client);
}

void Serial2TcpBase::handleData(void *arg, AsyncClient *client, void *data, size_t len)
{
#if 1
    __DBGS2T("arg=%p client=%p len=%u\n", arg, client, len);
#else
    PrintString str;
    if (data && len) {
        DumpBinary d(str);
        d.setPerLine(128);
        d.dump(data, std::min((size_t)128, len));
        str.trim();
    }
    __DBGS2T("arg=%p client=%p len=%u data=%s\n", arg, client, len, str.c_str());
#endif
    reinterpret_cast<Serial2TcpBase *>(arg)->_onData(client, data, len);
}

void Serial2TcpBase::handleDisconnect(void *arg, AsyncClient *client)
{
    __DBGS2T("arg=%p client=%p\n", arg, client);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, F("Disconnect"));
}

void Serial2TcpBase::handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
    __DBGS2T("arg=%p client=%p time=%u\n", arg, client, time);
    reinterpret_cast<Serial2TcpBase *>(arg)->_onDisconnect(client, PrintString(F("Timeout %u"), time));
}

void Serial2TcpBase::handleAck(void *arg, AsyncClient *client, size_t len, uint32_t time)
{
    // __DBGS2T("arg=%p client=%p len=%u time=%u\n", arg, client, len, time);
}

void Serial2TcpBase::handlePoll(void *arg, AsyncClient *client)
{
    // __DBGS2T("arg=%p client=%p\n", arg, client);
}

void Serial2TcpBase::_onSerialData(Stream &client)
{
    // __DBGS2T("type=%d len=%u\n", type, len);
}

void Serial2TcpBase::_onConnect(AsyncClient *client)
{
    // __DBGS2T("_onConnect client=%p\n", client);
}

void Serial2TcpBase::_onData(AsyncClient *client, void *data, size_t len)
{
    // __DBGS2T("_onData client=%p len=%u\n", client, len);
}

void Serial2TcpBase::_onDisconnect(AsyncClient *client, const String &reason)
{
    // __DBGS2T("_onDisconnect client=%p reason=%s\n", client, reason.c_str());
}

void Serial2TcpBase::_setClient(SerialHandler::Client *client)
{
    _client = client;
}

void Serial2TcpBase::_stopClient()
{
    __DBGS2T("_stopClient client=%p\n", _client);
    if (_client) {
        _client->stop();
    }
}

void Serial2TcpBase::_startClient()
{
    __DBGS2T("_startClient client=%p\n", _client);
    if (_client) {
        _client->start(SerialHandler::EventType::RW);
    }
}


Stream &Serial2TcpBase::getStream()
{
    return _client ? *_client : _serial;
}
