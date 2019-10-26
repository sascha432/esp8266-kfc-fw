/**
 * Author: sascha_lammers@gmx.de
 */

#if STK500V1

#include <LoopFunctions.h>
#include <ctype.h>
#include <Timezone.h>
#include <StreamString.h>
#include "at_mode.h"
#include "logger.h"
#include "serial_handler.h"
#include "STK500v1Programmer.h"
#if HTTP2SERIAL
#include "plugins/http2serial/http2serial.h"
#endif

STK500v1Programmer *stk500v1 = nullptr;

const char Command_SYNC[] PROGMEM = { STK500v1Programmer::Cmnd_STK_GET_SYNC, STK500v1Programmer::Sync_CRC_EOP };
const char Command_READ_SIGNATURE[] PROGMEM = { STK500v1Programmer::Cmnd_STK_READ_SIGN, STK500v1Programmer::Sync_CRC_EOP };
const char Command_ENTER_PROG_MODE[] PROGMEM = { STK500v1Programmer::Cmnd_STK_ENTER_PROGMODE, STK500v1Programmer::Sync_CRC_EOP };
const char Command_LEAVE_PROG_MODE[] PROGMEM = { STK500v1Programmer::Cmnd_STK_LEAVE_PROGMODE, STK500v1Programmer::Sync_CRC_EOP };
const char Response_INSYNC[] PROGMEM = { STK500v1Programmer::Resp_STK_INSYNC, STK500v1Programmer::Resp_STK_OK };

STK500v1Programmer::STK500v1Programmer(Stream &serial) : _serial(serial), _delayTimeout(0), _timeout(1000), _pageSize(128), _logging(LOG_DISABLED) {
    memcpy_P(_signature, PSTR("\x1e\x95\x0f"), 3);
    _pageBuffer = new uint8_t[_pageSize];
}

STK500v1Programmer::~STK500v1Programmer() {
    delete _pageBuffer;
}

void STK500v1Programmer::setFile(const String &filename) {
    _file.open(filename);
}

void STK500v1Programmer::begin(Callback_t cleanup) {

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

void STK500v1Programmer::end() {

    _clearReponse(100, [this]() {
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

void STK500v1Programmer::loopFunction() {
    stk500v1->_loopFunction();
}

void STK500v1Programmer::_readResponse(Callback_t success, Callback_t failure) {
    _callbackSuccess = success;
    _callbackFailure = failure;
}

void STK500v1Programmer::_loopFunction() {
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
                    _callbackFailure();
                }
                break;
            }
        }
        if (_readResponseTimeout && millis() > _readResponseTimeout) {
            _callbackFailure();
        }
    }
}

void STK500v1Programmer::_writePage(uint16_t wordAddress, uint16_t length, Callback_t success) {

    _sendCommandLoadAddress(wordAddress);

    _updatePosition();

    _readResponse([this, wordAddress, length, success]() {

        // _log.println(F("Write address loaded"));
        // _printResponse();

        _sendCommandProgPage(_pageBuffer, length);

        _readResponse(
            success,
            [this, wordAddress, length]() {

                _endPosition(F("WRITE ERROR"), true);

                _logPrintf_P(PSTR("Failed to write page %04x length %u"), wordAddress, length);
                _printResponse();

                _done(false);
            }
        );

    }, [this, wordAddress]() {

        _endPosition(F("ADDRESS ERROR"), true);

        _logPrintf_P(PSTR("Failed to load address %04x\n"), wordAddress);
        _printResponse();

        _done(false);
    });

}

void STK500v1Programmer::_verifyPage(uint16_t wordAddress, uint16_t length, Callback_t success) {

    success();
}

void STK500v1Programmer::_readFile(PageCallback_t callback, Callback_t success, Callback_t failure) {

    if (_file.isEOF()) {
        if (_pagePosition) {
            callback((_pageAddress >> 1), _pagePosition, success); // data left in page buffer
            _pagePosition = 0;
        }
        else {
            success();
        }
        return;
    }

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

            _logPrintf_P(PSTR("Read error occurred: %s\n"), _file.getErrorMessage());
            failure();
            return;
        }
        else if (length == 0) {
            if (!_file.isEOF()) {
                _endPosition(F("FILE READ ERROR"), true);

                _logPrintf_P(PSTR("Read length is zero, address %04x %04x, read %u\n"), _pageAddress, address, read);
                failure();
                return;
            }
        }

        uint16_t pageAddress = (address / _pageSize);
        if (_pageAddress != pageAddress) {  // new page
            // write and clear page buffer
            callback((_pageAddress >> 1), _pagePosition, [this, callback, success, failure]() {
                _readFile(callback, success, failure);
            });

            _clearPageBuffer();
            _pageAddress = pageAddress;
            _pagePosition = 0;

            // leave loop
            exit = true;
        }
        uint16_t pageOffset = address - (_pageAddress * _pageSize);
        memcpy(_pageBuffer + pageOffset, buffer, length);
        _pagePosition = pageOffset + length;

        if (_file.isEOF()) {
            break;
        }

    } while (!exit);
}

void STK500v1Programmer::_flash() {

    if (!_file.validate()) {
        PGM_P error = PSTR("Validation of the input file failed: ");
        _status(String(FPSTR(error)) + String(_file.getErrorMessage()) + String('\n'));
        _logPrintf_P(PSTR("%s%s"), error, _file.getErrorMessage());
        end();
        return;
    }
    _status(F("Input file validated\n"));
    _logPrintf_P(PSTR("Input file validated"));

    _delay(250, [this]() {

        _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));

        _reset();
        _sendCommand_P_repeat(Command_SYNC, sizeof(Command_SYNC), 3, 50);

        _retries = 12;
        _readResponse([this]() {
            _status(F("Connected...\n"));

            _logPrintf_P(PSTR("Sync complete"));
            _printResponse();

            _clearReponse(100, [this]() {

                _expectedResponse.clear();
                _expectedResponse.write(Resp_STK_INSYNC);
                _expectedResponse.write(_signature, 3);
                _expectedResponse.write(Resp_STK_OK);
                _sendCommand_P(Command_READ_SIGNATURE, sizeof(Command_READ_SIGNATURE));

                _readResponse([this]() {
                    Options_t options;

                    PrintString str;
                    str.printf_P(PSTR("Device signature = 0x%02x%02x%02x\n"), _signature[0], _signature[1], _signature[2]);
                    _status(str);

                    _logPrintf_P(PSTR("Signature verified"));
                    _printResponse();

                    memset(&options, 0, sizeof(options));
                    options.pageSizeHigh = (_pageSize >> 8) & 0xff;
                    options.pageSizeLow = _pageSize & 0xff;

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
                                [this](uint16_t address, uint16_t length, Callback_t success) {
                                    _writePage(address, length, success);
                                },
                                [this]() {

                                    _endPosition(F("Complete"), false);

                                    _logPrintf_P(PSTR("Programming completed"));

                                    _file.reset();
                                    _startPosition(F("Reading"));

                                    _readFile(
                                        [this](uint16_t address, uint16_t length, Callback_t success) {
                                            _verifyPage(address, length, success);
                                        },
                                        [this]() {

                                            _endPosition(F("Complete"), false);

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
                                            _logPrintf_P(PSTR("Verifying failed"));
                                            _done(false);
                                        }
                                    );

                                },
                                [this]() {
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
                _logPrintf_P(PSTR("Sync failed, retries left %u\n"), _retries);
                _printResponse();

                _clearReponse(100, [this]() {

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

void STK500v1Programmer::_delay(uint16_t time, Callback_t callback) {
    _delayTimeout += time;
    _callbackDelay = callback;
}

void STK500v1Programmer::_sendCommand_P_repeat(PGM_P command, uint8_t length, uint8_t num, uint16_t delayTime) {
    while(num--) {
        _sendCommand_P(command, length);
        delay(delayTime);
    }
}

void STK500v1Programmer::_sendCommandSetOptions(const STK500v1Programmer::Options_t &options) {
    _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));
    _logPrintf_P(PSTR("Sending set options, options length=%u\n"), sizeof(options));

    _serial.write(Cmnd_STK_SET_DEVICE);
    _serial.write(reinterpret_cast<const uint8_t *>(&options), sizeof(options));
    _serial.write(Sync_CRC_EOP);
    _response.clear();
    _setTimeout(_timeout);
}

void STK500v1Programmer::_sendCommandLoadAddress(uint16_t address) {
    _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));

    _serial.write(Cmnd_STK_LOAD_ADDRESS);
    _serial.write(address & 0xff);
    _serial.write((address >> 8) & 0xff);
    _serial.write(Sync_CRC_EOP);
    _response.clear();
    _setTimeout(_timeout);
}

void STK500v1Programmer::_sendCommandProgPage(uint8_t *data, uint16_t length) {
    _setResponse_P(Response_INSYNC, sizeof(Response_INSYNC));

    _serial.write(Cmnd_STK_PROG_PAGE);
    _serial.write((length >> 8) & 0xff);
    _serial.write(length & 0xff);
    _serial.write(0x46);
    _serial.write(data, length);
    _serial.write(Sync_CRC_EOP);
    _response.clear();
    _setTimeout(_timeout);
}

void STK500v1Programmer::_sendCommand_P(PGM_P command, uint8_t length) {
    if (_logging != LOG_DISABLED) {
        PrintString str(F("Sending (length=%u): "), length);
        while(length--) {
            auto byte = pgm_read_byte(command++);
            _serial.write(byte);
            str.printf_P(PSTR("%02x "), byte);
        }
        _logPrintf_P(PSTR("%s"), str.c_str());
    }
    _response.clear();
    _setTimeout(_timeout);
}

void STK500v1Programmer::_done(bool success) {
    PrintString str;
    str.printf_P(PSTR("Programming %s\n"), success ? PSTR("successful") : PSTR("failed"));
    _status(str);
    _logPrintf_P(PSTR("%s"), str.c_str());

    end();
}

void STK500v1Programmer::_clearReponse(uint16_t delayTime, Callback_t callback) {
    _delay(delayTime, [this, callback] {
        _response.clear();
        while (_serial.available()) {
            _serial.read();
        }
        callback();
    });
}

void STK500v1Programmer::_clearPageBuffer() {
    memset(_pageBuffer, 0, _pageSize);
}

void STK500v1Programmer::setSignature(char *signature) {
    if (*signature) {
        memcpy(_signature, signature, sizeof(_signature));
    }
}

bool STK500v1Programmer::getSignature(const char *mcu, char *signature) {
    if (!strncasecmp_P(mcu, PSTR("atmega"), 6)) {
        mcu += 6;
    }
    else if (!strncasecmp_P(mcu, PSTR("atm"), 3)) {
        mcu += 3;
    }
    else if (!strncasecmp_P(mcu, PSTR("at"), 2)) {
        mcu += 2;
    }

    if (!strcasecmp_P(mcu, "328p")) {
        memcpy_P(signature, PSTR("\x1e\x95\x0f"), 3);
    }
    else if (!strcasecmp_P(mcu, "328pb")) {
        memcpy_P(signature, PSTR("\x1e\x95\x16"), 3);
    }
    else {
        *signature = 0;
        return false;
    }
    return true;
}

void STK500v1Programmer::_setResponse_P(PGM_P response, uint8_t length) {
    _response.clear();
    _expectedResponse.clear();
    _expectedResponse.write_P(response, length);
}

void STK500v1Programmer::_setTimeout(uint16_t timeout) {
    _readResponseTimeout = millis() + timeout;
}

void STK500v1Programmer::_printBuffer(Print &str, const Buffer &buffer) {
    auto ptr = buffer.getConstChar();
    auto count = buffer.length();
    while(count--) {
        str.printf_P(PSTR("%02x "), *ptr++ & 0xff);
    }
}

void STK500v1Programmer::_printResponse() {
    if (_logging != LOG_DISABLED) {
        PrintString str(F("Response (length=%u)"), _response.length());
        if (_response.length()) {
            str.print(F(": "));
            _printBuffer(str, _response);
            if (_response != _expectedResponse) {
                str.print(F("expected "));
                _printBuffer(str, _expectedResponse);
            }
        }
        _logPrintf_P(PSTR("%s"), str.c_str());
    }
}

void STK500v1Programmer::_reset() {

    _logPrintf_P(PSTR("Sending reset signal..."));

    digitalWrite(STK500V1_RESET_PIN, LOW);
    pinMode(STK500V1_RESET_PIN, OUTPUT);
    digitalWrite(STK500V1_RESET_PIN, LOW);
    delay(1);
    pinMode(STK500V1_RESET_PIN, INPUT);
}

void STK500v1Programmer::_status(const String &message) {
#if HTTP2SERIAL
    auto http2server = Http2Serial::getConsoleServer();
    if (http2server) {
        if (http2server->getClients().length()) {
            WsClient::broadcast(http2server, nullptr, message.c_str(), message.length());
        }
    }
#endif
}

void STK500v1Programmer::_logPrintf_P(PGM_P format, ...) {
    if (_logging != LOG_DISABLED) {
        PrintString str;
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
    }
}

void STK500v1Programmer::_startPosition(const String &message) {
    _position = 0;
    _positionOld = 0;
    PrintString str(F("%s: | "), message.c_str());
    _status(str);
}

void STK500v1Programmer::_updatePosition() {
    String str;
    _position = _file.position() * PROGRESS_BAR_LENGTH / _file.size();
    if (_positionOld< _position) {
        while(_positionOld++ < _position) {
            str += '#';
        }
        _status(str);
    }
    // PrintString x(F("p %u %u\n"), (int)_file.position(), (int)_file.size());
    // _status(x);
}

void STK500v1Programmer::_endPosition(const String &message, bool error) {
    PrintString str;
    while(_positionOld++ < PROGRESS_BAR_LENGTH) {
        str += error ? 'x' : '#';
    }
    str.printf_P(PSTR(" | %s\n\n"), message.c_str());
    _status(str);
}

#endif
