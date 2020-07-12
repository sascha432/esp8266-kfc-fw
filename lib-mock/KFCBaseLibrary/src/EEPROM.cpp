/**
* Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#include <Arduino_compat.h>
#include "EEPROM.h"

uint8_t EERef::operator*() const 
{
    return _eeprom->__read(index); 
}

EERef &EERef::operator=(uint8_t in) 
{ 
    _eeprom->__write(index, in);
    return *this; 
}

EEPROMFile::EEPROMFile(uint16_t size) : _eeprom(nullptr), _position(0), _size(size), _eepromWriteCycles(nullptr), _cellLifetime(100000)
{
}

EEPROMFile::~EEPROMFile() 
{
    close();
    if (_eeprom) {
        delete []_eeprom;
    }
    if (_eepromWriteCycles) {
        delete []_eepromWriteCycles;
    }
}

int EEPROMFile::peek() 
{
    if (_position < _size) {
        return _eeprom[_position];
    }
    return -1;
}

int EEPROMFile::read() 
{
    uint8_t buf[1];
    auto len = read(buf, 1);
    return len == 1 ? buf[0] : -1;
}

int EEPROMFile::available() 
{
    return _size - _position;
}

void EEPROMFile::clear() 
{
    begin();
    memset(_eeprom, 0, _size);
    commit();
}

size_t EEPROMFile::length() const 
{
    return _size;
}


EEPtr EEPROMFile::getDataPtr() const 
{
    return EEPtr(0, this);
}

const uint8_t *EEPROMFile::getConstDataPtr() const
{
    return _eeprom;
}

EERef EEPROMFile::operator[](const int idx) 
{
    return EERef(idx, this);
}

uint8_t EEPROMFile::read(int idx) 
{
    return EERef(idx, this);
}

void EEPROMFile::write(int idx, uint8_t val) 
{
    EERef(idx, this) = val;
}

void EEPROMFile::update(int idx, uint8_t val) 
{
    EERef(idx, this).update(val);
}
 
uint8_t EEPROMFile::__read(int const address) 
{
    if (isProtected(address, AccessProtection::Type::READ, true)) {
        return 0xff;
    }
    if (_eepromWriteCycles[address] > _cellLifetime) {
        Serial.printf("address %u: %u > %u\n", address, _eepromWriteCycles[address], _cellLifetime);
        return 0xff;
    }
    if (std::binary_search(_damagedCell.begin(), _damagedCell.end(), (uint32_t)address)) {
        Serial.printf("address %u: marked as damaged\n", address);
        return 0xff;
    }
    return _eeprom[address];
}

void EEPROMFile::__write(int const address, uint8_t const val) 
{
    if (isProtected(address, AccessProtection::Type::WRITE, true)) {
        return;
    }
    _eeprom[address] = val;
    _eepromWriteCycles[address]++;
}

void EEPROMFile::__update(int const address, uint8_t const val)
{
    if (isProtected(address, AccessProtection::Type::WRITE, true)) {
        return;
    }
    if (_eeprom[address] != val) {
        write(address, val);
    }
}

bool EEPROMFile::begin(int size) 
{
    if (_eeprom) {
        delete []_eeprom;
    }
    if (_eepromWriteCycles) {
        delete []_eepromWriteCycles;
    }
    _eeprom = new uint8_t[_size];
    if (!_eeprom) {
        return false;
    }
    _eepromWriteCycles = new uint32_t[_size];
    if (!_eepromWriteCycles) {
        return false;
    }
    clearWriteCycles();
    bool result = true;
    do {
        FILE *fp;
        errno_t err = fopen_s(&fp, "eeprom.bin", "rb");
        if (err) {
            memset(_eeprom, 0, _size);
            if (commit()) {
                break;
            }
            result = false;
            break;
        }
        if (fread(_eeprom, _size, 1, fp) != 1) {
            perror("read");
            fclose(fp);
            result = false;
            break;
        }
        fclose(fp);

        err = fopen_s(&fp, "eeprom.cycles", "rb");
        if (err) {
            clearWriteCycles();
            break;
        }
        if (fread(_eepromWriteCycles, _size * sizeof(uint32_t), 1, fp) != 1) {
            clearWriteCycles();
        }
        fclose(fp);
    } while (false);
    return result;
}

void EEPROMFile::end() 
{
    if (_eeprom) {
        commit();
    }
}

bool EEPROMFile::commit() 
{
    bool result = false;
    do {
        FILE *fp;
        errno_t err = fopen_s(&fp, "eeprom.bin", "wb");
        if (err) {
            break;
        }
        if (fp) {
            if (fwrite(_eeprom, _size, 1, fp) != 1) {
                perror("write eeprom");
                break;
            }
            fclose(fp);
        }
        err = fopen_s(&fp, "eeprom.cycles", "wb");
        if (err) {
            break;
        }
        if (fp) {
            if (fwrite(_eepromWriteCycles, _size * sizeof(uint32_t), 1, fp) != 1) {
                perror("write cycles");
                break;
            }
            fclose(fp);
        }
        result = true;
    } while (false);
    return result;
}

void EEPROMFile::close() 
{
    end();
}

size_t EEPROMFile::position() const 
{
    return _position;
}

bool EEPROMFile::seek(long pos, SeekMode mode) 
{
    if (mode == SeekSet) {
        if (pos >= 0 && pos < _size) {
            _position = (uint16_t)pos;
            return true;
        }
    }
    else if (mode == SeekCur) {
        return seek(_position + pos, SeekSet);
    }
    else if (mode == SeekEnd) {
        return seek(_size - pos, SeekSet);
    }
    return false;
}

int EEPROMFile::read(uint8_t *buffer, size_t len) 
{
    if (_position + len >= _size) {
        len = _size - _position;
    }
    memcpy(buffer, _eeprom + _position, len);
    _position += (uint16_t)len;
    return len;
}

size_t EEPROMFile::write(uint8_t data) 
{
    return write(&data, sizeof(data));
}

size_t EEPROMFile::write(uint8_t *data, size_t len) 
{
    if (_position + len >= _size) {
        len = _size - _position;
    }
    memcpy(_eeprom + _position, data, len);
    _position += (uint16_t)len;
    return len;
}

void EEPROMFile::flush() 
{
}

size_t EEPROMFile::size() const 
{
    return _size;
}

bool EEPROMFile::isProtected(uint32_t index, AccessProtection::Type accessType, bool debugBreak) const
{
    auto iterator = std::find_if(_accessProtection.begin(), _accessProtection.end(), [index, debugBreak, accessType](const AccessProtection &item) {
        auto result = item.match(index);
        if (result != 0) {
            Serial.printf("EEPROM Access Violation at 0x%04x(%u), %s Access, Protection %s\n", index, index, AccessProtection::getType(accessType), AccessProtection::getType(result));
            if (debugBreak) {
                __debugbreak();
            }
        }
        return result != 0;
    });
    return (iterator != _accessProtection.end());
}

void EEPROMFile::addAccessProtection(AccessProtection::Type type, uint32_t offset, uint32_t length)
{
    if (length) {
        _accessProtection.emplace_back(type, offset, length);
    }
}

void EEPROMFile::clearAccessProtection()
{
    _accessProtection.clear();
}

void EEPROMFile::addDamagedCell(uint32_t index)
{
    _damagedCell.push_back(index);
    std::sort(_damagedCell.begin(), _damagedCell.end());
}

void EEPROMFile::clearDamagedCells()
{
    _damagedCell.clear();
}

void EEPROMFile::clearWriteCycles()
{
    memset(_eepromWriteCycles, 0, _size * sizeof(uint32_t));
}

void EEPROMFile::setCellLifetime(uint32_t cellLifetime)
{
    _cellLifetime = cellLifetime;
}

void EEPROMFile::dumpWriteCycles(Print &output, uint32_t start, uint32_t len, int cols)
{
    uint32_t end = start + len;
    typedef struct {
        uint32_t index;
        uint32_t count;
    } cycle_t;

    uint32_t min = ~0;
    uint32_t max = 0;
    uint64_t sum = 0;

    //std::vector<cycle_t> sorted;
    for (size_t i = start; i < end; i++) {
        //sorted.push_back(cycle_t({ i, _eepromWriteCycles[i] }));
        min = std::min(_eepromWriteCycles[i], min);
        max = std::max(_eepromWriteCycles[i], max);
        sum += _eepromWriteCycles[i];
    }
    //std::sort(sorted.begin(), sorted.end(), [](cycle_t a, cycle_t b) {
    //    return a.count > b.count;
    //});
    Serial.printf("--- EEPROM: min=%u, max=%u, avg=%.4f, sum=%.0f\n", min, max, sum / (double)_size, (double)sum);
    //for (size_t i = 0; i < limit; i++) {
    //    Serial.printf("%04x: %u\n", sorted[i].index, sorted[i].count);
    //}

    int nlc = 0;
    for (size_t i = start; i < end; i++) {
        Serial.printf("% 6d ", _eepromWriteCycles[i]);
        if (++nlc >= cols) {
            Serial.println();
            nlc = 0;
        }
    }
    Serial.println();
}

#endif
