/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <IntelHexFormat.h>
#include <Buffer.h>
#include <PrintString.h>

#ifndef STK500_HAVE_SOFTWARE_SERIAL
#define STK500_HAVE_SOFTWARE_SERIAL 0
#endif

#ifndef STK500_HAVE_SOFTWARE_SERIAL_PINS
#define STK500_HAVE_SOFTWARE_SERIAL_PINS D5, D6
#endif

#ifndef STK500_HAVE_FUSES
#define STK500_HAVE_FUSES 1
#endif

PROGMEM_STRING_DECL(stk500v1_log_file);
PROGMEM_STRING_DECL(stk500v1_sig_file);
PROGMEM_STRING_DECL(stk500v1_tmp_file);

class STK500v1Programmer {
public:
    typedef enum : uint8_t {
        Cmnd_STK_GET_SYNC =             0x30,
        Cmnd_STK_GET_SIGN_ON =          0x31,

        Cmnd_STK_SET_PARAMETER =        0x40,
        Cmnd_STK_GET_PARAMETER =        0x41,
        Cmnd_STK_SET_DEVICE =           0x42,
        Cmnd_STK_SET_DEVICE_EXT =       0x45,

        Cmnd_STK_ENTER_PROGMODE =       0x50,
        Cmnd_STK_LEAVE_PROGMODE =       0x51,
        Cmnd_STK_CHIP_ERASE =           0x52,
        Cmnd_STK_CHECK_AUTOINC =        0x53,
        Cmnd_STK_LOAD_ADDRESS =         0x55,
        Cmnd_STK_UNIVERSAL =            0x56,
        Cmnd_STK_UNIVERSAL_MULTI =      0x57,

        Cmnd_STK_PROG_FLASH =           0x60,
        Cmnd_STK_PROG_DATA =            0x61,
        Cmnd_STK_PROG_FUSE =            0x62,
        Cmnd_STK_PROG_LOCK =            0x63,
        Cmnd_STK_PROG_PAGE =            0x64,
        Cmnd_STK_PROG_FUSE_EXT =        0x65,

        Cmnd_STK_READ_FLASH =           0x70,
        Cmnd_STK_READ_DATA =            0x71,
        Cmnd_STK_READ_FUSE =            0x72,
        Cmnd_STK_READ_LOCK =            0x73,
        Cmnd_STK_READ_PAGE =            0x74,
        Cmnd_STK_READ_SIGN =            0x75,
        Cmnd_STK_READ_OSCCAL =          0x76,
        Cmnd_STK_READ_FUSE_EXT =        0x77,
        Cmnd_STK_READ_OSCCAL_EXT =      0x78
    } CommandsEnum_t;

    typedef enum : uint8_t {
        Resp_STK_ANY =                  0x00,
        Resp_STK_OK =                   0x10,
        Resp_STK_FAILED =               0x11,
        Resp_STK_UNKNOWN =              0x12,
        Resp_STK_NODEVICE =             0x13,
        Resp_STK_INSYNC =               0x14,
        Resp_STK_NOSYNC =               0x15
    } ResponseEnum_t;

    typedef enum : uint8_t {
        Sync_CRC_EOP =                  0x20
    } SyncEnum_t;

    typedef enum : uint8_t {
        TYPE_FLASH = 'F',
        TYPE_EEPROM = 'E',
    } ProgPageTypeEnum_t;

    typedef enum {
        FUSE_LOW,
        FUSE_HIGH,
        FUSE_EXT,
    } FuseEnum_t;

    struct Options_t {
        uint8_t deviceCode;
        uint8_t revision;
        uint8_t progType;
        uint8_t parMode;
        uint8_t polling;
        uint8_t selfTimed;
        uint8_t lockBytes;
        uint8_t fuseBytes;
        uint8_t flashPollVal1;
        uint8_t flashPollVal2;
        uint8_t eepromPollVal1;
        uint8_t eepromPollVal2;
        uint8_t pageSizeHigh;
        uint8_t pageSizeLow;
        uint8_t eepromSizeHigh;
        uint8_t eepromSizeLow;
        uint8_t flashSize4;
        uint8_t flashSize3;
        uint8_t flashSize2;
        uint8_t flashSize1;
    };

    enum LoggingEnum {
        LOG_DISABLED =      0,
        LOG_LOGGER =        1,
        LOG_SERIAL =        2,
        LOG_SERIAL2HTTP =   3,
        LOG_FILE =          4,
    };

    typedef std::function<void ()> Callback_t;
    typedef std::function<void (uint16_t address, uint16_t length, Callback_t success, Callback_t failure)> PageCallback_t;

    static const int PROGRESS_BAR_LENGTH = 100;

public:
    STK500v1Programmer(Stream &serial);
    virtual ~STK500v1Programmer();

    void setFile(const String &filename);

    void begin(Callback_t cleanup);
    void end();

    static void dumpLog(Stream &output);
    static void loopFunction();

    void setPageSize(uint16_t pageSize);

    // set default timeout
    void setTimeout(uint16_t timeout);

    void setLogging(int logging);

public:
    void setSignature(const char *signature);
    void setSignature_P(PGM_P signature);
    // void setFuseBytes(uint8_t low, uint8_t high, uint8_t extended);
    static bool getSignature(const char *mcu, char *signature);

private:
    static void _parseSignature(const char *str, char *signature);

private:
    void _reset();
    void _flash();
    void _serialWrite(uint8_t data);
    void _serialWrite(const uint8_t *data, uint8_t length);

    void _serialWrite(const char *data, uint8_t length);
    void _serialClear();
    void _serialRead();

    // asynchronous delay
    void _delay(uint16_t time, Callback_t callback);
    // send command N times and wait delay milliseconds
    // abort if receiving any data
    void _sendCommand_P_repeat(PGM_P command, uint8_t length, uint8_t num, uint16_t delay);

    // send command
    void _sendCommand_P(PGM_P command, uint8_t length);
    void _sendCommand(const char *command, uint8_t length);
    void _sendCommandEnterProgMode();
    void _sendCommandLeaveProgMode();
    void _sendCommandSetOptions(const Options_t &options);
    void _sendCommandLoadAddress(uint16_t address);
    void _sendCommandProgPage(const uint8_t *data, uint16_t length);
    void _sendCommandReadPage(const uint8_t *data, uint16_t length);
    #if STK500_HAVE_FUSES
    void _sendCommandReadFuseExt();
    void _sendCommandProgFuseExt(uint8_t fuseLow, uint8_t fuseHigh, uint8_t fuseExt);
    #endif
    void _setExpectedResponse_P(PGM_P response, uint8_t length);

    // read response with timeout
    void _readResponse(Callback_t success, Callback_t failure);
    // skip response
    void _skipResponse(Callback_t success, Callback_t failure);

    // set timeout for response
    // timeout = 0 will use the timeout previously set
    void _setResponseTimeout(uint16_t timeout);

    bool _compareResponse(const Buffer &respReceived, const Buffer &respExpected);

    void _printBuffer(Print &str, const Buffer &buffer);
    void _printResponse();

    void _done(bool success);
    void _clearResponse(uint16_t delay, Callback_t callback);
    void _clearPageBuffer();
    void _uploadCallback();
    void _readFile(PageCallback_t callback, Callback_t success, Callback_t failure);
    void _leaveProgrammingMode();
    void _writePage(uint16_t address, uint16_t length, Callback_t success, Callback_t failure);
    void _verifyPage(uint16_t address, uint16_t length, Callback_t success, Callback_t failure);

    // handle delay and responses
    void _loopFunction();

private:
    Stream &_serial;

private:
    uint32_t _flashStartTime;
    uint32_t _startTime;
    uint16_t _defaultTimeout;
    uint16_t _readResponseTimeout;
    Buffer _response;
    Buffer _expectedResponse;
    Callback_t _callbackSuccess;
    Callback_t _callbackFailure;
    uint32_t _delayTimeout;
    Callback_t _callbackDelay;
    Callback_t _callbackCleanup;
    uint8_t _retries;

private:
    char _signature[3];
    #if STK500_HAVE_FUSES
    char _fuseBytes[3];
    #endif
    IntelHexFormat _file;
    uint16_t _pageSize;
    uint8_t *_pageBuffer;
    uint16_t _pageAddress;
    uint16_t _pagePosition;
    uint16_t _verified;

private:
    void _status(const String &message);
    void _logPrintf_P(PGM_P format, ...) __attribute__((format(printf, 2, 3)));
    void _log(PGM_P format, ...) __attribute__((format(printf, 2, 3)));

    template<typename ..._Args>
    void _status(const __FlashStringHelper *format, _Args ...args);

    template<typename ..._Args>
    void _log(const __FlashStringHelper *format, _Args ...args);

private:
    LoggingEnum _logging;
    bool _atMode;

private:
    void _startPosition(const String &message);
    void _updatePosition();
    void _endPosition(const String &message, bool error);

private:
    uint8_t _position;
    uint8_t _positionOld;
    bool _success;
};

extern STK500v1Programmer *stk500v1;

inline STK500v1Programmer::~STK500v1Programmer()
{
    delete[] _pageBuffer;
}

inline void STK500v1Programmer::_readResponse(Callback_t success, Callback_t failure)
{
    _startTime = millis();
    _response.clear();
    _serialRead();
    _callbackSuccess = success;
    _callbackFailure = failure;
}

inline void STK500v1Programmer::_skipResponse(Callback_t success, Callback_t failure)
{
    success();
}

inline void STK500v1Programmer::_setExpectedResponse_P(PGM_P response, uint8_t length)
{
    _logPrintf_P(PSTR("Expected response length=%u"), length);
    _expectedResponse.clear();
    _expectedResponse.write_P(response, length);
}

inline void STK500v1Programmer::_delay(uint16_t time, Callback_t callback)
{
    _startTime = millis();
    _delayTimeout = time;
    _callbackDelay = callback;
}

inline void STK500v1Programmer::setFile(const String &filename)
{
    _file.open(filename);
}

inline void STK500v1Programmer::loopFunction()
{
    stk500v1->_loopFunction();
}

inline void STK500v1Programmer::setTimeout(uint16_t timeout)
{
    _defaultTimeout = timeout;
}

inline void STK500v1Programmer::setPageSize(uint16_t pageSize)
{
    _pageSize = pageSize;
}

inline void STK500v1Programmer::setLogging(int logging)
{
    _logging = (LoggingEnum)logging;
    if (_logging == LOG_FILE) {
        KFCFS.remove(FSPGM(stk500v1_log_file));
        KFCFS.open(FSPGM(stk500v1_log_file), fs::FileOpenMode::write).close(); // truncate
    }
}

inline void STK500v1Programmer::_serialWrite(const char *data, uint8_t length)
{
    _serialWrite(reinterpret_cast<const uint8_t *>(data), length);
}

inline void STK500v1Programmer::_serialClear()
{
    while(_serial.available()) {
        _serial.read();
    }
}

inline void STK500v1Programmer::_serialRead()
{
    while(_serial.available()) {
        _response.write(_serial.read());
    }
}

inline void STK500v1Programmer::_sendCommand(const char *command, uint8_t length)
{
    _sendCommand_P(command, length);
}

template<typename ..._Args>
void STK500v1Programmer::_status(const __FlashStringHelper *format, _Args ...args)
{
    PrintString str(format, args...);
    _status(str);
}

template<typename ..._Args>
void STK500v1Programmer::_log(const __FlashStringHelper *format, _Args ...args)
{
    _log(reinterpret_cast<PGM_P>(format), args...);
}
