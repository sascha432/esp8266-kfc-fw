/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// class to read and validate Intel Hex format
// overlapping or missing data is not detected
// record type data is supported only
// a maximum of one record or 255 byte is buffered in memory

#include "Arduino_compat.h"
#include "Buffer.h"

class IntelHexFormat {
public:
    class Record {
    public:
        Record() {
        }

        void clear() {
            _data.clear();
        }

        void setData(uint16_t address, uint8_t *data, uint8_t length) {
            _data.clear();
            _data.write(data, length);
            _address = address;
        }

        uint16_t getAddress() const {
            return _address;
        }

        void incrAddress(uint16_t position) {
            _address += position;
        }

        Buffer &getData() {
            return _data;
        }

        const char *getDataPtr() const {
            return _data.getConstChar();
        }

        bool isEmpty() const {
            return _data.length() == 0;
        }

        uint8_t length() const {
            return (uint8_t)_data.length();
        }

    private:
        Buffer _data;
        uint16_t _address;
    };

    typedef enum {
        RT_DATA =       0x00,
        RT_EOF =        0x01,
    } RecordTypeEnum_t;

    typedef enum {
        NONE,
        FAILED_TO_OPEN,
        READ_ERROR,
        INVALID_CHECKSUM,
        RECORD_TYPE_NOT_SUPPORTED,
        INVALID_CHARACTER,
        EOF_BEFORE_END,
    } ErrorEnum_t;

    IntelHexFormat();

    bool open(const String &filename);
    void close();

    // read file and validate format and checksum
    bool validate();

    // reads length byte into buffer and sets the address
    // returns number of bytes read or -1 if an error occurred
    int readBytes(char *buffer, size_t length, uint16_t &address);

    // true if end of file has been reached
    bool isEOF() const;

    // returns the last address in hex after calling validate()
    uint16_t getEndAddress() const;

    // returns address stored in _record
    uint16_t getCurAddress() const;

    // reset file to position 0
    void reset();

    // get last error message
    PGM_P getErrorMessage() const;

    ErrorEnum_t getError() const;

private:
    int _readHexByte();
    bool _readRecord(bool store);

    File _file;
    bool _eof;
    ErrorEnum_t _error;
    uint16_t _endAddress;

    Record _record;
};
