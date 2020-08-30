/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <IntelHexFormat.h>
#include <Buffer.h>
#include <PrintString.h>

PROGMEM_STRING_DECL(stk500v1_log_file);
PROGMEM_STRING_DECL(stk500v1_sig_file);
PROGMEM_STRING_DECL(stk500v1_tmp_file);

class STK500v1Programmer {
public:
    typedef enum : uint8_t {
        Cmnd_STK_GET_SYNC =             0x30,
        Cmnd_STK_SET_DEVICE =           0x42,
        Cmnd_STK_ENTER_PROGMODE =       0x50,
        Cmnd_STK_LEAVE_PROGMODE =       0x51,
        Cmnd_STK_LOAD_ADDRESS =         0x55,
        Cmnd_STK_PROG_FUSE =            0x62,
        Cmnd_STK_PROG_PAGE =            0x64,
        Cmnd_STK_PROG_FUSE_EXT =        0x65,
        Cmnd_STK_READ_PAGE =            0x74,
        Cmnd_STK_READ_SIGN =            0x75,
        Cmnd_STK_READ_FUSE_EXT =        0x77,
    } CommandsEnum_t;

    typedef enum : uint8_t {
        Resp_STK_OK =                   0x10,
        Resp_STK_INSYNC =               0x14,
        Resp_STK_NOSYNC =               0x15,
        Sync_CRC_EOP =                  0x20,
    } ResponseEnum_t;

    typedef enum : uint8_t {
        TYPE_FLASH = 'F',
        TYPE_EEPROM = 'E',
    } ProgPageTypeEnum_t;

    typedef enum {
        FUSE_LOW,
        FUSE_HIGH,
        FUSE_EXT,
    } FuseEnum_t;

    typedef struct {
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
    } Options_t;

    typedef enum {
        LOG_DISABLED =      0,
        LOG_LOGGER =        1,
        LOG_SERIAL =        2,
        LOG_SERIAL2HTTP =   3,
        LOG_FILE =          4,
    } LoggingEnum_t;

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

    void setPageSize(uint16_t pageSize) {
        _pageSize = pageSize;
    }

    void setTimeout(uint16_t timeout) {
        _timeout = timeout;
    }

    void setLogging(int logging) {
        _logging = (LoggingEnum_t)logging;
        if (_logging == LOG_FILE) {
            KFCFS.open(FSPGM(stk500v1_log_file), fs::FileOpenMode::write).close(); // truncate
        }
    }

public:
    void setSignature(const char *signature);
    void setSignature_P(PGM_P signature);
    void setFuseBytes(uint8_t low, uint8_t high, uint8_t extended);
    static bool getSignature(const char *mcu, char *signature);

private:
    static void _parseSignature(const char *str, char *signature);

private:
    void _reset();
    void _flash();
    void _serialWrite(uint8_t data);
    void _serialWrite(const uint8_t *data, uint8_t length);
    void _delay(uint16_t time, Callback_t callback);
    void _sendCommand_P_repeat(PGM_P command, uint8_t length, uint8_t num, uint16_t delay);
    void _sendCommand_P(PGM_P command, uint8_t length);
    void _sendCommand(const char *command, uint8_t length);
    void _sendCommandSetOptions(const Options_t &options);
    void _sendCommandLoadAddress(uint16_t address);
    void _sendCommandProgPage(const uint8_t *data, uint16_t length);
    void _sendCommandReadPage(const uint8_t *data, uint16_t length);
    void _sendProgFuseExt(uint8_t fuseLow, uint8_t fuseHigh, uint8_t fuseExt);
    void _setResponse_P(PGM_P response, uint8_t length);
    void _loopFunction();
    void _readResponse(Callback_t success, Callback_t failure);
    void _skipResponse(Callback_t success, Callback_t failure);
    void _printBuffer(Print &str, const Buffer &buffer);
    void _printResponse();
    void _setTimeout(uint16_t timeout);
    void _done(bool success);
    void _clearResponse(uint16_t delay, Callback_t callback);
    void _clearPageBuffer();
    void _uploadCallback();
    void _readFile(PageCallback_t callback, Callback_t success, Callback_t failure);
    void _leaveProgrammingMode();
    void _writePage(uint16_t address, uint16_t length, Callback_t success, Callback_t failure);
    void _verifyPage(uint16_t address, uint16_t length, Callback_t success, Callback_t failure);

private:
    Stream &_serial;

private:
    uint8_t _retries;
    unsigned long _readResponseTimeout;
    Buffer _response;
    Buffer _expectedResponse;
    Callback_t _callbackSuccess;
    Callback_t _callbackFailure;
    unsigned long _delayTimeout;
    Callback_t _callbackDelay;
    Callback_t _callbackCleanup;
    uint16_t _timeout;

private:
    char _signature[3];
    char _fuseBytes[3];
    IntelHexFormat _file;
    uint16_t _pageSize;
    uint8_t *_pageBuffer;
    uint16_t _pageAddress;
    uint16_t _pagePosition;
    uint16_t _verified;
    unsigned long _startTime;

private:
    void _status(const String &message);
    void _logPrintf_P(PGM_P format, ...);

private:
    LoggingEnum_t _logging;

private:
    void _startPosition(const String &message);
    void _updatePosition();
    void _endPosition(const String &message, bool error);

private:
    uint8_t _position;
    uint8_t _positionOld;
};

extern STK500v1Programmer *stk500v1;
