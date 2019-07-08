/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <algorithm>

class dynamic_bitset {
private:
    class bit_helper {
    public:
        bit_helper(dynamic_bitset *bitset, uint8_t index);

        bit_helper &operator=(unsigned int value) {
            _bitset->set(_index, (bool)value);
            return *this;
        }
        bit_helper &operator=(const bit_helper &helper) {
            _bitset->set(_index, (bool)helper);
            return *this;
        }

        operator bool() const {
            return _bitset->test(_index);
        }

    private:
        uint8_t _index;
        dynamic_bitset *_bitset;
    };

public:
    static const uint8_t defaultSize = sizeof(int32_t) << 3;

    dynamic_bitset();
    dynamic_bitset(uint8_t maxSize);
    dynamic_bitset(uint8_t size, uint8_t maxSize);
    dynamic_bitset(uint32_t value, uint8_t size, uint8_t maxSize);
    ~dynamic_bitset();

    dynamic_bitset & operator =(unsigned int value) {
        setValue(value);
        return *this;
    }
    dynamic_bitset & operator =(const dynamic_bitset &bitset) {
		setMaxSize(bitset._maxSize);
        setSize(bitset._size);
        memcpy(_buffer, bitset._buffer, ((_maxSize + 7) >> 3));
        return *this;
    }

	dynamic_bitset & operator =(const char * str) {
		setString(str);
		return *this;
    }

    bit_helper operator [](unsigned int index) {
        return bit_helper(this, index);
    }

    bool operator [](unsigned int index) const {
        return test(index);
    }

    void setBytes(uint8_t *bytes, uint8_t len);

    const uint8_t *getBytes() const;
    String getBytesAsString() const;

    void setValue(uint32_t value);

    const uint32_t getValue() const;

#if defined(__int64_t_defined)
    void setValueInt64(uint64_t value) {
        memcpy(_buffer, &value, std::min((int)sizeof(value), ((_maxSize + 7) >> 3)));
    }

    uint64_t getValueInt64() {
        uint64_t value;
        memcpy(&value, _buffer, std::min((int)sizeof(value), ((_maxSize + 7) >> 3)));
        return value;
    }
#endif

    void setString(const String &str) {
        setString(str.c_str(), str.length());
    }
	void setString(const char *str) {
        setString(str, strlen(str));
    }
    void setString(const char *str, size_t len);

    void setSize(uint8_t size);
    const uint8_t size() const;

    void setMaxSize(uint8_t maxSize);
    const uint8_t getMaxSize() const;

    String toString() const;
    void set(uint8_t pos, bool value = true);
    bool test(uint8_t pos) const;

private:
    void _init();

private:
    uint8_t _size;
    uint8_t _maxSize;
    public:
    uint8_t *_buffer;
};

/*
#include <assert.h>

int main() {

	dynamic_bitset budf(0, 64, 64);
	budf.setInt32Ofs(0b11011011000000111110000111110000, 4);
	budf.setInt32Ofs(                                0b11111000000110110110000000000000, 0);
    assert(budf.toString() == "1101101100000011111000011111000011111000000110110110000000000000");

	dynamic_bitset bcda;
    bcda.setSize(9);
    bcda = 0b010100011;
    bcda[0] = 0;
    bcda[0] = bcda[1];
    // std::cout << bcda[0] << std::endl;

    dynamic_bitset bcde;
    bcde = bcda;
    assert(bcde.toString() == "010100011");
    assert(std::string("010100011") == bcde.toString());

    dynamic_bitset bcdf(0, 19);
    bcda.setSize(19);
    bcda = 0b1011111110000000010;
    for (int i = 0; i < 19; i++) {
        bcdf[i] = bcda[i];
    }
    assert(bcda.toString() == "1011111110000000010");

    uint8_t bytes[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    const char *str = "01000101001000101011011011010101010000100101010100101110110001010101010110111101101000010101";

    int len = strlen(str);
    dynamic_bitset ffg;
    ffg.setMaxSize(len);  // 1101101000010101
    ffg.setSize(len);
    ffg = str;
    assert(str == ffg.toString());

    dynamic_bitset ffz(0, 64, 64);
   ffz.setBytes(bytes, sizeof(bytes));
    assert(ffz.toString() == "1111111111111111111111111111111111111111111111111111111111111111");
    ffz = "110101";
	assert(ffz.toString() == "0000000000000000000000000000000000000000000000000000000000110101");

    dynamic_bitset bb(0b10000000000000000000000010100000, 32, 32);
    // 01234567890123456789012345678912

    assert(bb.test(31) == true);
    assert(bb.test(5) == true);
    assert(bb.test(7) == true);
    assert(bb[31] == true);
    assert(bb[7] == true);
    assert(bb[5] == true);
    assert(bb[5] == true);

    dynamic_bitset cc(64);
    cc.setSize(64);
    cc.setBytes(bytes, sizeof(bytes));
    cc.set(63, false);
    assert(cc.toString() == "0111111111111111111111111111111111111111111111111111111111111111");
    cc[63] = 1;
    assert(cc.toString() == "1111111111111111111111111111111111111111111111111111111111111111");

    dynamic_bitset bc(0b01010101010101010101010101010101, 32, 32);
    // 01234567890123456789012345678912

    assert(bc.toString() == "01010101010101010101010101010101");
    bc.setSize(31);
    assert(bc.toString() == "1010101010101010101010101010101");

    dynamic_bitset bf;

    bf = 0b01100101;
    char buf[] = {bf[7], bf[6], bf[5], bf[4], bf[3], bf[2], bf[1], bf[0], 0};
    for (int i = 0; i < 8; i++) {
        buf[i] += 48;
    }
    assert(strcmp(buf, "01100101") == 0);

}


*/
