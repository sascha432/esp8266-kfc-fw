// pattern_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include  <Arduino_compat.h>
#include "dyn_bitset.h"
 #include <assert.h>

void tests() {

    dynamic_bitset set1;
    set1.setSize(9);
    set1 = 0b010100011;
    assert(set1.toString() == "");

    set1.setMaxSize(9);
    set1 = 0b010100011;
    assert(set1.toString() == "010100011");

    dynamic_bitset set2 = set1;
    assert(set1.toString() == "010100011");

    dynamic_bitset set3(0, 19);
    set3.setSize(19);
    set3 = 0b1011111110000000010;
    assert(set3.toString() == "1011111110000000010");

    dynamic_bitset set4(0xe38e15, 27, 27);
    assert(set4.toString() == "000111000111000111000010101");

    dynamic_bitset set5(27, 27);
    set5 = 0xe38e15;
    assert(set5.toString() == "000111000111000111000010101");

    int len = set5.size();
    int bit = 0;
    const char *ptr = "101010000111000111000111000";
    while (len--) {
        bool value = *ptr == '1';
        assert(set5.test(bit) == value);
        assert(set5[bit] == value);
        ptr++;
        bit++;
    }

    dynamic_bitset set6((uint32_t)0, 1, 1);
    assert(set6.toString() == "0");

    dynamic_bitset set7 = set5;

    len = 10;
    set7.setSize(len);
    while (len--) {
        set7.set(len, 0);
    }
    assert(set7.toString() == "0000000000");
    len = 11;
    set7.setSize(len);
    while (len--) {
        set7[len] = 1;
    }
    assert(set7.toString() == "11111111111");

    dynamic_bitset set8(5, 10);

    assert(set8.toString() == "00000");

    set8.setSize(3);
    set8.setString("111");
    assert(set8.toString() == "111");

    set8.setSize(20);
    set8.setString("010101010101010101");
    assert(set8.toString() == "0101010101");

    set8.setSize(8);
    set8.setString("111");
    assert(set8.toString() == "00000111");

    set8.setSize(10);
    assert(set8.toString() == "0000000111");
}

int main() {

    //tests();

    dynamic_bitset pattern;

    dynamic_bitset pattern2(0xe38e15, 27, 27);
    printf("%s\n", pattern2.toString().c_str());

    pattern.fromString("101010111000111000111000");
    printf("\n");

    printf("%s\n", pattern.toString().c_str());
    for (int i = 0; i < pattern.size(); i++) {
        printf("%d - %d\n", i, pattern.test(i) ? 1 : 0);
    }

    printf("\n\n%s\n\n%s\n", pattern.generateCode("pattern2").c_str(), pattern.generateCode("pattern2", true).c_str());

    pattern.fromString("110011001100000000111111000000111111000000111111000000");
    printf("\n\n%s\n\n%s\n", pattern.generateCode("pattern3").c_str(), pattern.generateCode("pattern3", true).c_str());

    return 0;
}
