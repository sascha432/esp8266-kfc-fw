// test_config.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Arduino_compat.h>
#include <Buffer.h>
#include <algorithm>
#include <map>
#include "Configuration.h"
#include "ConfigurationParameter.h"

char tmp[] = { 0x00, 0x00 };

EEPROMFile EEPROM;

typedef struct {
    uint8_t flag1 : 1;
    uint8_t flag2 : 1;
    uint8_t flag3 : 1;
} ConfigSub2_t;

// structure is used for code completion only
typedef struct {

    char string1[100];
    char blob[10];
    int32_t int32;
    uint8_t val1;
    uint16_t val2;
    uint32_t val3;
    uint64_t val4;
    IPAddress ip;
    struct {
        char string1[100];
        char string2[100];
    } sub1;
    ConfigSub2_t sub2;
    
} Configuration_t;

int main() {

    EEPROM.begin();
#if 0
    memcpy(EEPROM.getDataPtr(), tmp, sizeof(tmp));
    EEPROM.commit();
    Configuration config2(28, 4096);
    if (!config2.read()) {
        printf("Failed to read configuration\n");
    }
    config2.dump(Serial);
    return 0;
#endif

#if 0
    EEPROM.clear();
    EEPROM.commit();
#endif
    Configuration config(0, 256);

    if (!config.read()) {
        printf("Failed to read configuration\n");
    }

    config.dump(Serial);

    auto ptr = config._H_STR(Configuration_t().string1);
    printf("%s\n", ptr);
    uint16_t length = 0;
    ConfigSub2_t *ptr2;
    if (config.getBinary(_H(Configuration_t().sub2), ptr2, length)) {
        printf("%d: ", length);
        printf("%d ", ptr2->flag1);
        printf("%d ", ptr2->flag2);
        printf("%d\n", ptr2->flag3);
    }
    ConfigSub2_t data2;
    if (config.getStruct(_H(Configuration_t().sub2), data2)) {
        printf("%d ", data2.flag1);
        printf("%d ", data2.flag2);
        printf("%d\n", data2.flag3);
    }
    auto data = config.get<uint8_t>(_H(Configuration_t().sub2));
    printf("%d\n", data);
     
    config.release();

    config.setString(_H(Configuration_t().string1), "bla2");
    ConfigSub2_t s = { 1, 0 , 1 };
    config.set(_H(Configuration_t().sub2), s);

    config.setBinary(_H(Configuration_t().blob), "blob\x1\x2\x3", 7);

    char *writeableStr = config.getWriteableString(_H(Configuration_t().sub1.string1), sizeof Configuration_t().sub1.string1);
    strcpy(writeableStr, "dummy");

    auto &int32 = config.getWriteable<int32_t>(_H(Configuration_t().int32));
    int32 = -12345678;

    auto ip = config._H_W_GET_IP(Configuration_t().ip);
    ip = (uint32_t)IPAddress(1, 2, 3, 4);

    auto test = config._H_GET(Configuration_t().int32);
    printf("%d\n", test);

    IPAddress ip2 = config._H_GET_IP(Configuration_t().ip);
    printf("%s\n", ip2.toString().c_str());

    config._H_SET(Configuration_t().val1, 0x80);
    config._H_SET(Configuration_t().val2, 0x8070);
    config._H_SET(Configuration_t().val3, 0x80706050);
    config._H_SET(Configuration_t().val4, 0x8070605040302010);

    config.dump(Serial);

    if (config.isDirty()) {
        config.write();
    }

    config.dump(Serial);

    config.release();

    config.dump(Serial);

    return 0;
}
