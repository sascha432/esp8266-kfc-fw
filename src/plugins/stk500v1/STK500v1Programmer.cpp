/**
 * Author: sascha_lammers@gmx.de
 */

#include <LoopFunctions.h>
#include <ctype.h>
#include <StreamString.h>
#include "at_mode.h"
#include "logger.h"
#include "serial_handler.h"
#include "STK500v1Programmer.h"
#include "blink_led_timer.h"
#if HTTP2SERIAL
#include "plugins/http2serial/http2serial.h"
#endif

STK500v1Programmer *stk500v1 = nullptr;

const char Command_SYNC[] PROGMEM = { STK500v1Programmer::Cmnd_STK_GET_SYNC, STK500v1Programmer::Sync_CRC_EOP };
const char Command_READ_SIGNATURE[] PROGMEM = { STK500v1Programmer::Cmnd_STK_READ_SIGN, STK500v1Programmer::Sync_CRC_EOP };
const char Command_ENTER_PROG_MODE[] PROGMEM = { STK500v1Programmer::Cmnd_STK_ENTER_PROGMODE, STK500v1Programmer::Sync_CRC_EOP };
const char Command_LEAVE_PROG_MODE[] PROGMEM = { STK500v1Programmer::Cmnd_STK_LEAVE_PROGMODE, STK500v1Programmer::Sync_CRC_EOP };
const char Response_INSYNC[] PROGMEM = { STK500v1Programmer::Resp_STK_INSYNC, STK500v1Programmer::Resp_STK_OK };

PROGMEM_STRING_DEF(stk500v1_log_file, "/stk500v1/debug.log");
PROGMEM_STRING_DEF(stk500v1_sig_file, "/stk500v1/atmega.csv");
PROGMEM_STRING_DEF(stk500v1_tmp_file, "/stk500v1/firmware_tmp.hex");

STK500v1Programmer::STK500v1Programmer(Stream &serial) : _serial(serial), _delayTimeout(0), _timeout(400), _pageSize(128), _logging(LOG_DISABLED)
{
    _signature[0] = 0x1e;
    _signature[1] = 0x95;
    _signature[2] = 0x0f;
    _fuseBytes[FUSE_LOW] = 0xff;
    _fuseBytes[FUSE_HIGH] = 0xda;
    _fuseBytes[FUSE_EXT] = 0xff;
    _pageBuffer = new uint8_t[_pageSize];
    BlinkLEDTimer::setBlink(__LED_BUILTIN, 500);
}

STK500v1Programmer::~STK500v1Programmer()
{
    delete _pageBuffer;
}

void STK500v1Programmer::setFile(const String &filename)
{
    _file.open(filename);
}

void STK500v1Programmer::begin(Callback_t cleanup)
{
    _callbackCleanup = cleanup;

    _pageAddress = 0;
    _pagePosition = 0;
    _clearPageBuffer();

    if (&_serial == &KFC_SERIAL_PORT) {
        // disable all output and handlers while flashing
        StreamString nul;
        disable_at_mode(nul);
        LoopFunctions::remove(SerialHandler::serialLoop);
        DEBUG_HELPER_SILENT();
    }

    _readResponseTimeout = 0;
    LoopFunctions::add(STK500v1Programmer::loopFunction);

    _flash();
}

void STK500v1Programmer::end()
{
    _clearResponse(100, [this]() {
        LoopFunctions::remove(STK500v1Programmer::loopFunction);

        if (&_serial == &KFC_SERIAL_PORT) {
            DEBUG_HELPER_INIT();
            LoopFunctions::add(SerialHandler::serialLoop);
            StreamString nul;
            enable_at_mode(nul);
        }

        _response.clear();
        _expectedResponse.clear();
        _file.close();

        _status(F("Done\n"));

        Scheduler.addTimer(1000, false, [this](EventScheduler::TimerPtr timer) {
            _callbackCleanup();
        });
    });
}

void STK500v1Programmer::loopFunction()
{
    stk500v1->_loopFunction();
}

void STK500v1Programmer::_readResponse(Callback_t success, Callback_t failure)
{
    _callbackSuccess = success;
    _callbackFailure = failure;
}

void STK500v1Programmer::_skipResponse(Callback_t success, Callback_t failure)
{
    success();
}

void STK500v1Programmer::_loopFunction()
{
    if (_delayTimeout && millis() > _delayTimeout) {
        _delayTimeout = 0;
        _callbackDelay();
    }
    else {
        while (_serial.available()) {
            auto byte = _serial.read();
            _response.write(byte);
            if (_response.length() == _expectedResponse.length()) {
                if (_response == _expectedResponse) {
                    _callbackSuccess();
                }
                else {
                    _logPrintf_P(PSTR("Response length %u != %u"), _response.length(), _expectedResponse.length());
                    _callbackFailure();
                }
                break;
            }
        }
        if (_readResponseTimeout && millis() > _readResponseTimeout) {
            _logPrintf_P(PSTR("Response timeout"));
            _callbackFailure();
        }
    }
}

void STK500v1Programmer::_writePage(uint16_t address, uint16_t length, Callback_t success, Callback_t failure)
{
    BlinkLEDTimer::setBlink(__LED_BUILTIN, address % 2 == 0 ? BlinkLEDTimer::OFF : BlinkLEDTimer::SOLID);
    _sendCommandLoadAddress(address);

    _updatePosition();

    uint8_t tmp[256]; // copy current page buffer
    memcpy(tmp, _pageBuffer, length);

    _readResponse([this, address, length, success, failure, tmp]() {

        _sendCommandProgPage(tmp, length);

        _readResponse(success, [this, address, length, failure]() {
            _logPrintf_P(PSTR("Write page %u length %u failed"), address, length);
            failure();
        });

    }, failure);
}

void STK500v1Programmer::_verifyPage(uint16_t address, uint16_t length, Callback_t success, Callback_t failure)
{
    BlinkLEDTimer::setBlink(__LED_BUILTIN, address % 2 == 0 ? BlinkLEDTimer::OFF : BlinkLEDTimer::SOLID);
    _sendCommandLoadAddress(address);

    _updatePosition();

    uint8_t tmp[256]; // copy current page buffer
    memcpy(tmp, _pageBuffer, length);

    _readResponse([this, address, length, success, failure, tmp]() {

        _sendCommandReadPage(tmp, length);

        _readResponse([this, success, length]() {
            _verified += length;
            success();
        }, [this, failure, address, length]() {
            _logPrintf_P(PSTR("Verify page %u length %u failed"), address, length);
            failure();
        });

    }, failure);
}

void STK500v1Programmer::_readFile(PageCallback_t callback, Callback_t success, Callback_t failure)
{
    bool exit = false;
    do {
        char buffer[16];
        uint16_t address;
        uint16_t read = sizeof(buffer);
        if (_pagePosition != _pageSize && read > _pageSize - _pagePosition) { // limit length to space left in page buffer
            read = _pageSize - _pagePosition;
        }
        auto length = _file.readBytes(buffer, read, address);
        if (length == -1) {
            _endPosition(F("FILE READ ERROR"), true);
            _logPrintf_P(PSTR("Read error occurred: %s"), _file.getErrorMessage());
            failure();
            return;
        }

        uint16_t pageAddress = (address / _pageSize);
        if (_pageAddress != pageAddress || length == 0) {  // new page, page buffer is filled already
            if (length == 0) {
                // write last page
                callback(_pageAddress, _pagePosition, success, failure);
            }
            else {
                // write page buffer and continue reading
                callback(_pageAddress, _pagePosition, [this, callback, success, failure]() {
                    _readFile(callback, success, failure);
                }, failure);
            }

            // clear page buffer
            _clearPageBuffer();
            _pageAddress = pageAddress;
            _pagePosition = 0;

            // leave loop after storing data in buffer
            exit = true;
        }
        uint16_t pageOffset = address - (_pageAddress * _pageSize);
        memcpy(_pageBuffer + pageOffset, buffer, length);
        _pagePosition = pageOffset + length;

    } while (!exit);
}

void STK500v1Programmer::_serialWrite(uint8_t byte)
{
    uint8_t retries = 5;
    while(retries--) {
        if (_serial.write(byte) == 1) {
            break;
        }
        delay(10);
        _serial.clearWriteError();
    }
    _serial.flush(); // wait until byte has been sent
}

void STK500v1Programmer::_serialWrite(const uint8_t *data, uint8_t length)
{
    uint8_t count = length;
    auto ptr = data;
    while(count--) {
        _serialWrite(*ptr++);
    }
}

void STK500v1Programmer::_flash()
{
    struct tm *tm;
    auto now = time(nullptr);
    char temp[64];

    _startTime = millis();
    tm = localtime(&now);
    strftime_P(temp, sizeof(temp), PSTR("%FT%TZ"), tm);
    _logPrintf_P(PSTR("--- %s"), temp);

    if (!_file.validate()) {
        PrintString str(F("Validation of the input file failed: %s"), _file.getErrorMessage());
        _logPrintf_P(PSTR("%s"), str.c_str());
        str += '\n';
        _status(str);
        end();
        return;
    }

    PrintString str(F("Input file validated. %u bytes to write..."), _file.getEndAddress());
    _logPrintf_P(PSTR("%s"), str.c_str());
    str += '\n';
    _status(str);

    _delay(250, [this]() {

        _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));

        _reset();
        _sendCommand_P_repeat(Command_SYNC, sizeof(Command_SYNC), 3, 50);

        _retries = 12;
        _readResponse([this]() {
            _status(F("Connected to bootloader...\n"));

            _logPrintf_P(PSTR("Sync complete"));
            _printResponse();

            _clearResponse(100, [this]() {

                _expectedResponse.clear();
                _expectedResponse.write(Resp_STK_INSYNC);
                _expectedResponse.write(_signature, 3);
                _expectedResponse.write(Resp_STK_OK);
                _sendCommand_P(Command_READ_SIGNATURE, sizeof(Command_READ_SIGNATURE));

                _readResponse([this]() {

                    PrintString str;
                    str.printf_P(PSTR("Device signature = 0x%02x%02x%02x\n"), _signature[0], _signature[1], _signature[2]);
                    _status(str);

                    _logPrintf_P(PSTR("Signature verified"));
                    _printResponse();

                    // TODO does not get any response and times out
                    // _sendProgFuseExt(_fuseBytes[FUSE_LOW], _fuseBytes[FUSE_HIGH], _fuseBytes[FUSE_EXT]);

                    _skipResponse([this]() {
                        Options_t options;

                        // _logPrintf_P(PSTR("Fuse bits written"));
                        // _printResponse();

                        options = {};
                        options.pageSizeHigh = highByte(_pageSize);
                        options.pageSizeLow = lowByte(_pageSize);

                        _sendCommandSetOptions(options);

                        _readResponse([this]() {

                            _logPrintf_P(PSTR("Options set"));
                            _printResponse();

                            _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));
                            _sendCommand_P(Command_ENTER_PROG_MODE, sizeof(Command_ENTER_PROG_MODE));

                            _readResponse([this]() {

                                _file.reset();
                                _startPosition(F("\nWriting"));

                                _logPrintf_P(PSTR("Entered programming mode"));
                                _printResponse();

                                _readFile(
                                    [this](uint16_t address, uint16_t length, Callback_t success, Callback_t failure) {
                                        _writePage(address, length, success, failure);
                                    },
                                    [this]() {

                                        _endPosition(F("Complete"), false);
                                        _logPrintf_P(PSTR("Programming completed"));

                                        // reset file and page buffer
                                        _file.reset();
                                        _pageAddress = 0;
                                        _pagePosition = 0;
                                        _verified = 0;
                                        _clearPageBuffer();

                                        _startPosition(F("Reading"));

                                        _readFile(
                                            [this](uint16_t address, uint16_t length, Callback_t success, Callback_t failure) {
                                                _verifyPage(address, length, success, failure);
                                            },
                                            [this]() {

                                                _endPosition(F("Complete"), false);
                                                PrintString str(F("%u bytes verified\n"), _verified);
                                                _status(str);

                                                _logPrintf_P(PSTR("Verification completed"));

                                                _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));
                                                _sendCommand_P(Command_LEAVE_PROG_MODE, sizeof(Command_LEAVE_PROG_MODE));

                                                _readResponse([this]() {

                                                    _logPrintf_P(PSTR("Left programming mode"));
                                                    _done(true);

                                                }, [this]() {

                                                    _logPrintf_P(PSTR("Failed to leave programming mode"));
                                                    _printResponse();
                                                    _done(false);
                                                });

                                            },
                                            [this]() {
                                                _endPosition(F("READ ERROR"), true);

                                                _logPrintf_P(PSTR("Verifying failed"));
                                                _done(false);
                                            }
                                        );

                                    },
                                    [this]() {
                                        _endPosition(F("WRITE ERROR"), true);

                                        _logPrintf_P(PSTR("Uploading failed"));
                                        _done(false);
                                    }
                                );

                            }, [this]() {
                                _logPrintf_P(PSTR("Failed to enter programming mode"));
                                _printResponse();
                                _done(false);
                            });

                        }, [this]() {
                            _logPrintf_P(PSTR("Failed to set options"));
                            _printResponse();
                            _done(false);
                        });

                    }, [this]() {
                        _logPrintf_P(PSTR("Failed to write fuse bits"));
                        _printResponse();
                        _done(false);
                    });

                }, [this]() {
                    _logPrintf_P(PSTR("Failed to verify signature"));
                    _printResponse();
                    _done(false);
                });

            });

        }, [this]() {
            if (_retries-- == 0) {
                _done(false);
            }
            else {
                _logPrintf_P(PSTR("Sync failed, retries left %u"), _retries);
                _printResponse();

                _clearResponse(100, [this]() {

                    if (_retries % 4 == 0) {
                        _reset();
                        _delay(100, [this]() {

                            _sendCommand_P_repeat(Command_SYNC, sizeof(Command_SYNC), 3, 50);
                            _sendCommand_P(Command_SYNC, sizeof(Command_SYNC));
                        });
                    }
                    else {
                        _sendCommand_P(Command_SYNC, sizeof(Command_SYNC));
                    }
                });
            }
        });
    });
}

void STK500v1Programmer::_delay(uint16_t time, Callback_t callback)
{
    _delayTimeout += time;
    _callbackDelay = callback;
}

void STK500v1Programmer::_sendCommand_P_repeat(PGM_P command, uint8_t length, uint8_t num, uint16_t delayTime)
{
    while(num--) {
        _sendCommand_P(command, length);
        delay(delayTime);
    }
}

void STK500v1Programmer::_sendProgFuseExt(uint8_t fuseLow, uint8_t fuseHigh, uint8_t fuseExt)
{
    _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));
    _logPrintf_P(PSTR("Sending prog fuse ext, f:l=%02x,h=%02x, e:%02x"), fuseLow, fuseHigh, fuseExt);

    const char buf[5] = { Cmnd_STK_PROG_FUSE_EXT, fuseLow, fuseHigh, fuseExt, Sync_CRC_EOP };

    _sendCommand(buf, sizeof(buf));

    // _serialWrite(Cmnd_STK_PROG_FUSE_EXT);
    // _serialWrite(fuseLow);
    // _serialWrite(fuseHigh);
    // _serialWrite(fuseExt);
    // _serialWrite(Sync_CRC_EOP);
    // _response.clear();
    // _setTimeout(_timeout);
}

void STK500v1Programmer::_sendCommandSetOptions(const STK500v1Programmer::Options_t &options)
{
    _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));
    _logPrintf_P(PSTR("Sending set options, options length=%u"), sizeof(options));

    _serialWrite(Cmnd_STK_SET_DEVICE);
    _serialWrite(reinterpret_cast<const uint8_t *>(&options), sizeof(options));
    _serialWrite(Sync_CRC_EOP);
    _response.clear();
    _setTimeout(_timeout);
}

void STK500v1Programmer::_sendCommandLoadAddress(uint16_t address)
{
    _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));

    uint16_t wordAddress = ((address * _pageSize) >> 1);    // address is the page

    _serialWrite(Cmnd_STK_LOAD_ADDRESS);
    _serialWrite(lowByte(wordAddress));
    _serialWrite(highByte(wordAddress));
    _serialWrite(Sync_CRC_EOP);
    _response.clear();
    _setTimeout(_timeout);
}

void STK500v1Programmer::_sendCommandProgPage(const uint8_t *data, uint16_t length)
{
    _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));

    _serialWrite(Cmnd_STK_PROG_PAGE);
    _serialWrite(highByte(length));
    _serialWrite(lowByte(length));
    _serialWrite(TYPE_FLASH);
    _serialWrite(data, length);
    _serialWrite(Sync_CRC_EOP);
    _response.clear();
    _setTimeout(_timeout * 2);
}

void STK500v1Programmer::_sendCommandReadPage(const uint8_t *data, uint16_t length)
{
    _expectedResponse.clear();
    _expectedResponse.write(Resp_STK_INSYNC);
    // _expectedResponse.write((uint8_t)highByte(length));
    // _expectedResponse.write((uint8_t)lowByte(length));
    // _expectedResponse.write(TYPE_FLASH);
    _expectedResponse.write(data, length);
    _expectedResponse.write(Resp_STK_OK);

    _serialWrite(Cmnd_STK_READ_PAGE);
    _serialWrite(highByte(length));
    _serialWrite(lowByte(length));
    _serialWrite(TYPE_FLASH);
    _serialWrite(Sync_CRC_EOP);
    _response.clear();
    _setTimeout(_timeout * 2);
}

void STK500v1Programmer::_sendCommand_P(PGM_P command, uint8_t length)
{
    if (_logging != LOG_DISABLED) {
        PrintString str(F("Sending (length=%u): "), length);
        while(length--) {
            auto byte = pgm_read_byte(command++);
            _serialWrite(byte);
            str.printf_P(PSTR("%02x "), byte);
        }
        _logPrintf_P(PSTR("%s"), str.c_str());
    }
    _response.clear();
    _setTimeout(_timeout);
}

void STK500v1Programmer::_sendCommand(const char *command, uint8_t length)
{
    if (_logging != LOG_DISABLED) {
        PrintString str(F("Sending (length=%u): "), length);
        while(length--) {
            auto byte = *command++;
            _serialWrite(byte);
            str.printf_P(PSTR("%02x "), byte);
        }
        _logPrintf_P(PSTR("%s"), str.c_str());
    }
    _response.clear();
    _setTimeout(_timeout);
}

void STK500v1Programmer::_done(bool success)
{
    PrintString str;
    str.printf_P(PSTR("Programming %s\n"), success ? PSTR("successful") : PSTR("failed"));
    _status(str);
    _logPrintf_P(PSTR("%s"), str.c_str());

    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOLID);

    end();
}

void STK500v1Programmer::_clearResponse(uint16_t delayTime, Callback_t callback)
{
    _delay(delayTime, [this, callback] {
        _response.clear();
        while (_serial.available()) {
            _serial.read();
        }
        callback();
    });
}

void STK500v1Programmer::_clearPageBuffer()
{
    memset(_pageBuffer, 0, _pageSize);
}

void STK500v1Programmer::setSignature(const char *signature)
{
    if (*signature) {
        memcpy(_signature, signature, sizeof(_signature));
    }
}

void STK500v1Programmer::setSignature_P(PGM_P signature)
{
    memcpy_P(_signature, signature, sizeof(_signature));
}

void STK500v1Programmer::setFuseBytes(uint8_t low, uint8_t high, uint8_t extended)
{
    _fuseBytes[FUSE_LOW] = low;
    _fuseBytes[FUSE_HIGH] = high;
    _fuseBytes[FUSE_EXT] = extended;
}

void STK500v1Programmer::dumpLog(Stream &output)
{
    auto file = SPIFFS.open(FSPGM(stk500v1_log_file), fs::FileOpenMode::read);
    if (file) {
        while(file.available()) {
            output.print((char)file.read());
        }
        output.println();
    }
}

void STK500v1Programmer::_parseSignature(const char *str, char *signature)
{
    auto _long = strtoul(str, nullptr, 16);
    signature[0] = (_long >> 16) & 0xff;
    signature[1] = (_long >> 8) & 0xff;
    signature[2] = _long & 0xff;
}

bool STK500v1Programmer::getSignature(const char *mcu, char *signature)
{
    if (!strncasecmp_P(mcu, PSTR("0x"), 2)) {
        _parseSignature(mcu + 2, signature);
    }
    else {
        if (!strncasecmp_P(mcu, PSTR("atmega"), 6)) {
            mcu += 6;
        }
        else if (!strncasecmp_P(mcu, PSTR("atm"), 3)) {
            mcu += 3;
        }
        else if (!strncasecmp_P(mcu, PSTR("at"), 2)) {
            mcu += 2;
        }

        if (!strcasecmp_P(mcu, PSTR("328p"))) {
            memcpy_P(signature, PSTR("\x1e\x95\x0f"), 3);
        }
        else {
            auto file = SPIFFS.open(FSPGM(stk500v1_sig_file), fs::FileOpenMode::read);
            if (file) {
                while(file.available()) {
                    auto nameStr = file.readStringUntil(',');
                    auto signatureStr = file.readStringUntil('\n');
                    if (nameStr.equalsIgnoreCase(mcu) && signatureStr.length()) {
                        _parseSignature(signatureStr.c_str(), signature);
                        return true;
                    }
                }
            }
            *signature = 0;
            return false;
        }
    }
    return true;
}

void STK500v1Programmer::_setResponse_P(PGM_P response, uint8_t length)
{
    // _logPrintf_P(PSTR("Expected response length=%u"), length);
    _response.clear();
    _expectedResponse.clear();
    _expectedResponse.write_P(response, length);
}

void STK500v1Programmer::_setTimeout(uint16_t timeout)
{
    // _logPrintf_P(PSTR("Set timeout=%u"), timeout);
    _readResponseTimeout = millis() + timeout;
}

void STK500v1Programmer::_printBuffer(Print &str, const Buffer &buffer)
{
    auto ptr = buffer.getConstChar();
    auto count = buffer.length();
    const size_t maxLength = 8;
    if (count > maxLength) {
        count = maxLength;
    }
    while(count--) {
        str.printf_P(PSTR("%02x "), *ptr++ & 0xff);
    }
    if (buffer.length() > maxLength) {
        str.print(F("..."));
    }
}

void STK500v1Programmer::_printResponse()
{
    if (_logging != LOG_DISABLED) {
        PrintString str(F("Response (length=%u)"), _response.length());
        if (_response.length()) {
            str.print(F(": "));
            _printBuffer(str, _response);
        }
        if (_response != _expectedResponse) {
            str.print(F(" expected "));
            _printBuffer(str, _expectedResponse);
        }
        _logPrintf_P(PSTR("%s"), str.c_str());
    }
}

void STK500v1Programmer::_reset()
{
    _logPrintf_P(PSTR("Sending reset signal..."));

    digitalWrite(STK500V1_RESET_PIN, LOW);
    pinMode(STK500V1_RESET_PIN, OUTPUT);
    digitalWrite(STK500V1_RESET_PIN, LOW);
    delay(1);
    pinMode(STK500V1_RESET_PIN, INPUT);
}

void STK500v1Programmer::_status(const String &message)
{
#if HTTP2SERIAL
    auto http2server = Http2Serial::getConsoleServer();
    if (http2server) {
        if (http2server->getClients().length()) {
            WsClient::broadcast(http2server, nullptr, message.c_str(), message.length());
        }
    }
#endif
}

void STK500v1Programmer::_logPrintf_P(PGM_P format, ...)
{
    if (_logging != LOG_DISABLED) {
        PrintString str;

        str.printf_P(PSTR("%05lu "), millis() - _startTime);

        va_list arg;
        va_start(arg, format);
        str.vprintf(format, arg);
        va_end(arg);

        if (_logging == LOG_LOGGER) {
            Logger_notice(str);
        }
        else if (_logging == LOG_SERIAL) {
            MySerial.println(str);
        }
        else if (_logging == LOG_SERIAL2HTTP) {
            str += '\n';
            _status(str);
        }
        else if (_logging == LOG_FILE) {
            auto file = SPIFFS.open(FSPGM(stk500v1_log_file), fs::FileOpenMode::append);
            if (file) {
                file.println(str);
            }
        }
    }
}

void STK500v1Programmer::_startPosition(const String &message)
{
    _position = 0;
    _positionOld = 0;
    PrintString str(F("%s: | "), message.c_str());
    _status(str);
}

void STK500v1Programmer::_updatePosition()
{
    String str;
    _position = _file.getCurAddress() * (PROGRESS_BAR_LENGTH - 1) / _file.getEndAddress();
    if (_positionOld < _position) {
        while(_positionOld++ < _position) {
            str += '#';
        }
        _status(str);
    }
}

void STK500v1Programmer::_endPosition(const String &message, bool error)
{
    PrintString str;
    while(_positionOld++ < PROGRESS_BAR_LENGTH) {
        str += error ? 'x' : '#';
    }
    str.printf_P(PSTR(" | %s\n\n"), message.c_str());
    _status(str);
}
