/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Arduino_compat.h"

#define E2END 4095

class EEPROMFile;

typedef EEPROMFile EEPROMClass;

// EERef//EEPtr are for tracking write cycles per cell, read/access violations ewtc...

struct EERef {

    EERef(const int index, EEPROMClass *eeprom) : index(index), _eeprom(eeprom) {}

    //Access/read members.
    uint8_t operator*() const;
    operator uint8_t() const { 
        return **this; 
    }
    operator int() const { 
        return **this; 
    }

    //Assignment/write members.
    EERef &operator=(const EERef &ref) { return *this = *ref; }
    EERef &operator=(uint8_t in);
    EERef &operator +=(uint8_t in) { return *this = **this + in; }
    EERef &operator -=(uint8_t in) { return *this = **this - in; }
    EERef &operator *=(uint8_t in) { return *this = **this * in; }
    EERef &operator /=(uint8_t in) { return *this = **this / in; }
    EERef &operator ^=(uint8_t in) { return *this = **this ^ in; }
    EERef &operator %=(uint8_t in) { return *this = **this % in; }
    EERef &operator &=(uint8_t in) { return *this = **this & in; }
    EERef &operator |=(uint8_t in) { return *this = **this | in; }
    EERef &operator <<=(uint8_t in) { return *this = **this << in; }
    EERef &operator >>=(uint8_t in) { return *this = **this >> in; }

    EERef &update(uint8_t in) { return  in != *this ? *this = in : *this; }

    /** Prefix increment/decrement **/
    EERef &operator++() { return *this += 1; }
    EERef &operator--() { return *this -= 1; }

    /** Postfix increment/decrement **/
    uint8_t operator++ (int) {
        uint8_t ret = **this;
        return ++(*this), ret;
    }

    uint8_t operator-- (int) {
        uint8_t ret = **this;
        return --(*this), ret;
    }

    int index; //Index of current EEPROM cell.
    EEPROMClass *_eeprom;
};

struct EEPtr {

    EEPtr(const int index, EEPROMClass *eeprom) : index(index), _eeprom(eeprom) {}
    EEPtr(const int index, const EEPROMClass *eeprom) : index(index), _eeprom(const_cast<EEPROMClass *>(eeprom)) {}

    operator int() const { 
        return index; 
    }
    EEPtr &operator=(int in) { return index = in, *this; }

    EERef operator[](const int idx) {
        return EERef(index + idx, _eeprom);
    }

    //Iterator functionality.
    bool operator!=(const EEPtr &ptr) { return index != ptr.index; }
    EERef operator*() { return EERef(index, _eeprom); }

    EEPtr &operator+=(int num) {
        index += num;
        return *this;
    }
    EEPtr &operator-=(int num) {
        index -= num;
        return *this;
    }

    /** Prefix & Postfix increment/decrement **/
    EEPtr &operator++() { return ++index, *this; }
    EEPtr &operator--() { return --index, *this; }
    EEPtr operator++ (int) { return index++, *this; }
    EEPtr operator-- (int) { return index--, *this; }

    int index; //Index of current EEPROM cell.
    EEPROMClass *_eeprom;
};

class EEPROMFile : public Stream {
public:
    class AccessProtection {
    public:
        enum Type {
            NONE = 0,
            READ = 0x01,
            WRITE = 0x02,
            RW = 0x03
        };

        AccessProtection(Type type, uint32_t start, uint32_t end) : _type(type), _start(start), _end(end) {
        }

        static const char *getType(Type type)
        {
            if ((type & RW) == RW) {
                return "RW";
            }
            else if (type & READ) {
                return "Read";
            }
            else if (type & WRITE) {
                return "Read";
            }
            return "None";
        }

        size_t getOfs() const {
            return _start;
        }

        size_t length() const {
            return _end - _start;
        }

        Type match(uint32_t index) const {
            return (index >= _start && index < _end) ? _type : Type::NONE;
        }

    private:
        Type _type;
        uint32_t _start;
        uint32_t _end;
    };

public:
    EEPROMFile(uint16_t size = E2END + 1);
    ~EEPROMFile();

    void clear();
    size_t length() const;
    
    EEPtr getDataPtr() const;
    const uint8_t *getConstDataPtr() const;

    EERef operator[](const int idx);

    uint8_t read(int idx);
    void write(int idx, uint8_t val);
    void update(int idx, uint8_t val);

private:
    friend EERef;

    uint8_t __read(int const address);
    void __write(int const address, uint8_t const val);
    void __update(int const address, uint8_t const val);
public:

    int peek();
    int read();
    int available();

    bool begin(int size = -1);
    void end();
    bool commit();
    void close();

    size_t position() const;
    bool seek(long pos, SeekMode mode);
    bool seek(long pos) {
        return seek(pos, SeekSet);
    }
    int read(uint8_t *buffer, size_t len);
    size_t write(uint8_t data);
    size_t write(uint8_t *data, size_t len);
    void flush();
    size_t size() const;

    template<typename T>
    T &get(int const address, T &t) {
        if (address < 0 || address + sizeof(T) > _size)
            return t;

        auto dst = (uint8_t *)&t;
        for (size_t i = address; i < address + sizeof(T); i++) {
            *dst++ = read(i);
        }
        return t;
    }

    template<typename T>
    const T &put(int const address, const T &t) {
        if (address < 0 || address + sizeof(T) > _size) {
            return t;
        }
        auto src = (const uint8_t *)&t;
        for (size_t i = address; i < address + sizeof(T); i++) {
            write(i, *src++);
        }
        return t;
    }

    // add read/write or RW protection to cells
    // read will return 0xff if the cell is protected
    void addAccessProtection(AccessProtection::Type type, uint32_t offset, uint32_t length);
    void clearAccessProtection();

    // mark a cell as damaged, read will return 0xff
    void addDamagedCell(uint32_t index);
    // clear all cells that are marked as damaged
    void clearDamagedCells();

    // set lifetime of a cell. if write cycles exceed this value, read() will return 0xff
    void setCellLifetime(uint32_t cellLifetime);
    // dump write cycles of cells between start and end, add new line every cols
    void dumpWriteCycles(Print &output, uint32_t start, uint32_t end, int cols);
    // set write cycles for all cells to 0
    void clearWriteCycles();

 private:
     bool isProtected(uint32_t index, AccessProtection::Type accessType, bool debugBreak = false) const;

private:
    uint8_t *_eeprom;
    uint16_t _position;
    uint16_t _size;

private:
    uint32_t *_eepromWriteCycles;
    uint32_t _cellLifetime;
    std::vector<AccessProtection> _accessProtection;
    std::vector<uint32_t> _damagedCell;
};

extern EEPROMClass EEPROM;
