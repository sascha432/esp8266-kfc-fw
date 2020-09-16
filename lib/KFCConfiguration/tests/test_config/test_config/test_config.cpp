// test_config.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Arduino_compat.h>
#include <Buffer.h>
#include <FS.h>
#include <IntelHexFormat.h>
#include <algorithm>
#include <map>
#include <misc.h>
#include <forward_list>
#include <deque>
#include <stack>
#include "Configuration.h"
#include "ConfigurationParameter.h"
#include <chrono>

bool start_out = false;
int total = 0;
int del_num = 0;
std::map<void *, size_t> memptrs;
//
//void *operator new(size_t size)
//{
//    void *ptr = malloc(size);
//    if (start_out) {
//        start_out = false;
//        total += size;
//        memptrs[ptr] = size;
//        Serial.printf("new=%u (%u) %p\n", size, total, ptr);
//        start_out = true;
//    }
//    return ptr;
//}
//
//void operator delete(void *p) {
//    if (start_out) {
//        start_out = false;
//        auto size = memptrs[p];
//        total -= size;
//        del_num++;
//        Serial.printf("delete=%u (%u) #%u\n", size, total, del_num);
//        start_out = true;
//    }
//    free(p);
//}

char tmp[] = { 0x00, 0x00 };

typedef struct {
    uint8_t flag1 : 1;
    uint8_t flag2 : 1;
    uint8_t flag3 : 1;
} ConfigSub2_t;

typedef uint8_t blob_t[10];

typedef uint32_t ConfigFlags_t;

struct ConfigFlags {
    ConfigFlags_t isFactorySettings : 1;
    ConfigFlags_t isDefaultPassword : 1; //TODO disable password after 5min if it has not been changed
    ConfigFlags_t ledMode : 1;
    ConfigFlags_t wifiMode : 2;
    ConfigFlags_t atModeEnabled : 1;
    ConfigFlags_t hiddenSSID : 1;
    ConfigFlags_t softAPDHCPDEnabled : 1;
    ConfigFlags_t stationModeDHCPEnabled : 1;
    ConfigFlags_t webServerMode : 2;
#if defined(ESP8266)
    ConfigFlags_t webServerPerformanceModeEnabled : 1;
#endif
    ConfigFlags_t ntpClientEnabled : 1;
    ConfigFlags_t syslogProtocol : 3;
    ConfigFlags_t mqttMode : 2;
    ConfigFlags_t mqttAutoDiscoveryEnabled : 1;
    ConfigFlags_t restApiEnabled : 1;
    ConfigFlags_t serial2TCPMode : 3;
    ConfigFlags_t hueEnabled : 1;
    ConfigFlags_t useStaticIPDuringWakeUp : 1;
    int counter;
};

typedef struct {
    uint16_t a;
    uint16_t b;
    uint16_t c;
} sval_t;

// structure is used for code completion only
typedef struct {

    char string1[100];
    uint8_t val1;
    char string2[5];
    sval_t sval;
    uint16_t val2;
    char string3[12];
    uint32_t val3;
    char string4[32];
    uint64_t val4;
    char string5[16];
    float val5;
    //IPAddress ip;
    //struct {
    //    char string1[100];
    //    char string2[100];
    //} sub1;
    //ConfigSub2_t sub2;
    //blob_t blob2;
    //ConfigFlags flags;
    
} Configuration_t;


#include <assert.h>

#if 0

#include "push_pack.h"


#include "PrintHtmlEntitiesString.h"



uint8_t flash[4096] = {};

void spi_flash_read(size_t ofs, uint32_t *buf, size_t len) {
    ofs -= 0x40201000;
    //Serial.printf("spi_flash_read %u %u\n", ofs, len);
    memcpy(buf, flash + ofs, len);
}

void spi_flash_write(size_t ofs, uint32_t *buf, size_t len) {
    ofs -= 0x40201000;
    //Serial.printf("spi_flash_write %u %u\n", ofs, len);
    memcpy(flash + ofs, buf, len);
}

#endif




static constexpr uint16_t test1 = constexpr_crc16_update("test", constexpr_strlen("test"));
#define _HHHH(name) constexpr_crc16_update(name, constexpr_strlen(name))
static constexpr uint16_t test2 = _HHHH("test");

#define _H_PTR(name) ({static const uint16_t handle = 100; &handle;})

const char *loadStringConfig(uint16_t handle) {
    return nullptr;
}
void storeStringConfig(uint16_t handle, const char *) {}

#if 0

#undef _H
#undef CONFIG_GET_HANDLE_STR
#define _H_DEFINED_KFCCONFIGURATIONCLASSES                  1
#define _H(name)                                            constexpr_crc16_update(_STRINGIFY(name), constexpr_strlen(_STRINGIFY(name)))
#define CONFIG_GET_HANDLE_STR(name)                         constexpr_crc16_update(name, constexpr_strlen(name))

#define CREATE_STRING_GETTER_SETTER(class_name, name, len) \
    static constexpr size_t k##name##MaxLength = len; \
    static constexpr uint16_t k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static const char *get##name() { return loadStringConfig(k##name##ConfigHandle); } \
    static void set##name(const char *str) { storeStringConfig(k##name##ConfigHandle, str); } 

enum class ModeType : uint8_t {
    MIN = 0,
    DISABLED = MIN,
    UNSECURE,
    SECURE,
    MAX
};

#define DUMP_STRUCT_VAR(name) { output.print(_STRINGIFY(name)); output.print("="); output.println(this->name); }

enum class QosType : uint8_t {
    MIN = 0,
    AT_MODE_ONCE = 0,
    AT_LEAST_ONCE = 1,
    EXACTLY_ONCE = 2,
    MAX,
    DEFAULT = 0xff,
};
typedef struct __attribute__packed__ {
    uint16_t port{ 1883 };
    uint8_t keepalive{ 15 };
    union __attribute__packed__ {
        // uint8_t mode: 5;
        // ModeType mode_enum: 5;
        uint8_t mode;
        ModeType mode_enum{ ModeType::UNSECURE };
        //TODO suppress warning
    };
    union __attribute__packed__ {
        uint8_t qos;
        QosType qos_enum{ QosType::EXACTLY_ONCE };
        //TODO suppress warning for  TTClient::<anonymous struct>::<anonymous union>::qos_enum' is too small to hold all v
        // uint8_t qos: 2;
        // QosType qos_enum: 2;
    };
    union __attribute__packed__ {
        uint8_t flags{ 0b00000101 };
        struct __attribute__packed__ {
            uint8_t auto_discovery : 1;
            uint8_t auto_discoveryx : 1;
            uint8_t auto_discoveryy : 1;
        };
    };

    template<class T>
    void dump(Print &output) {
        output.printf_P(PSTR("config handle=%04x\n"), T::kConfigStructHandle);
        DUMP_STRUCT_VAR(port);
        DUMP_STRUCT_VAR(keepalive);
        DUMP_STRUCT_VAR(qos);
        DUMP_STRUCT_VAR(auto_discovery);
    }

} MqttConfig_t;

template<typename T, const uint16_t _handle>
class ConfigBla {
public:
    static constexpr uint16_t handle = _handle;
    void printHandle() {
        Serial.printf("%04x\n", handle);
    }

};

namespace Sy {

    struct Dev {
        typedef struct {
            int a;
        } Config_t;
    };

}

class ConfigBlaX : public ConfigBla<Sy::Dev::Config_t, _H(Sy::Dev::Config_t)>
{
public:

    CREATE_STRING_GETTER_SETTER(ConfigBlaX, Hostname, 128);
};

String prettyname(const char *s) {
    String tmp;
    tmp = "CONFIG_HANDLE_";
    tmp += s;
    tmp.toUpperCase();
    tmp.replace('.', '_');
    tmp.replace('(', '_');
    tmp.replace(')', '_');
    while (tmp.indexOf("__") != -1) {
        tmp.replace("__", "_");
    }
    return tmp;
}


#define CREATE_HANDLE(name) Serial.printf("#define %- 64.64s 0x%04x // _H(%s)\n", prettyname(name).c_str(), _H(name), name);

#endif

int main() {

    ESP._enableMSVCMemdebug();
    DEBUG_HELPER_INIT();

#if 1
    EEPROM.clear();
#endif
    EEPROM.begin();
    //EEPROM.clear();

    //EEPROM.addAccessProtection(EEPROMFile::AccessProtection::RW, 0, 512);
    //EEPROM.addAccessProtection(EEPROMFile::AccessProtection::RW, 512 + 1024, 1024);

    Configuration config(0, 1024);
    if (!config.read()) {
        Serial.println("Read error, settings defaults");
        config._H_SET_STR(Configuration_t().string1, "test1");
        config._H_SET(Configuration_t().val1, 100);
        config._H_SET_STR(Configuration_t().string2, "test2");
        config._H_SET(Configuration_t().sval, sval_t({0x8877, 0x5544}));
        config._H_SET_STR(Configuration_t().string3, "test3");
        config._H_SET(Configuration_t().val2, 10000);
        config._H_SET(Configuration_t().val3, 100000000);
        config._H_SET_STR(Configuration_t().string4, "test4");
        config._H_SET(Configuration_t().val4, 10000000000000000);
        config._H_SET_STR(Configuration_t().string5, "test5");
        config._H_SET(Configuration_t().val5, -12345.678f);
        config.dump(Serial);
        config.write();
    }
    config.dump(Serial);
    auto &ref = config._H_W_GET(Configuration_t().val1);
    ref++;
    config.dump(Serial);

//    EEPROM.commit();

    return 0;

    config._H_SET(Configuration_t().sval, sval_t({ 0x1122, 0x3344 }));

    //auto str = config._H_STR(Configuration_t().string1);

    config._H_SET_STR(Configuration_t().string1, "testlonger1");
    config._H_SET_STR(Configuration_t().string1, "short");
    config._H_SET_STR(Configuration_t().string1, "testlonger1testlonger1");
    config.dump(Serial);

    config.write();

    config.dump(Serial);

    return 0;

#if 0

  //  auto flags = config._H_GET(Configuration_t().flags);
  //  //flags = ConfigFlags();
  ///*  flags.counter++;
  //  flags.atModeEnabled = true;
  //  flags.isDefaultPassword = true;
  //  flags.stationModeDHCPEnabled = true;
  //  flags.wifiMode = 3;
  //  flags.webServerMode = 1;*/


  //  //"2702C001"
  //  //"3F03C001"
  //  //"BF03C001"
  //  //"BF0B C001"
  //  //"BF8B C201"

  //  flags.counter++;
  //  config._H_SET(Configuration_t().flags, flags);

  //  flags = config._H_GET(Configuration_t().flags);

  //  flags.counter++;
  //  config._H_SET(Configuration_t().flags, flags);

  //  flags = config._H_GET(Configuration_t().flags);

  //  flags.counter++;
  //  config._H_SET(Configuration_t().flags, flags);

  //  //auto mzstring = config._H_STR(Configuration_t().string1);


  //  //config._H_SET_STR(Configuration_t().string1, emptyString);

  //  char data[200] = {};

  //  config.setBinary(_H(binary), data, 0);
  //  config.setBinary(_H(binary), data, 0);

    config.write();

    return 0;
#endif

#if 0
    {
        auto &param = config.getParameterT<int>(_H(int));
        config.makeWriteable(param);
        param.set(123);
    }
    {
        auto &param = config.getParameterT<char *>(_H(str4));
        config.makeWriteable(param, 32);
        param.set("test4");
    }
    {
        auto &param = config.getParameterT<char *>(_H(str2));
        if (param) {
            config.makeWriteable(param, 32);
            param.set("test2");
        }
    }
#endif

#if 0
    auto &tmp = config.getWriteable<int>(_H(int));
    tmp++;

    //config.getWriteable<int>(_H(int2))++;

    String str = config.getString(_H(str1));

    str += '1';
    config.setString(_H(str1), str.c_str());

    //config.discard();

    str += '2';
    config.setString(_H(str1), str.c_str());
#endif

#if 0
    config.setString(_H(str1new), "");

    //config.setString(_H(str2), "str2_long");
    //config.setString(_H(str3), "str3");
    config.write();


    return 0;
#endif

#if 0
    auto file = SPIFFS.open(F("C:/Users/sascha/Documents/PlatformIO/Projects/kfc_fw/fwbins/blindsctrl/kfcfw_config_KFCD6ECD8_b2789_20191222_154625.json"), "r");
    if (file) {
        config.importJson(file);
        file.close();

        //config.dump(Serial);
        //config.write();
    }

    config.dump(Serial);
    config.write();
    return 0;
#endif

#if 0
    auto ptr = config._H_STR(Configuration_t().string1);
    printf("%s\n", ptr);
    if (config.exists<ConfigSub2_t>(_H(Configuration_t().sub2))) {
        auto data = config.get<uint8_t>(_H(Configuration_t().sub2));
        printf("%d\n", data);
    }
     
    config.release();

    config.setBinary(_H(Configuration_t().blob), "\x1\x2\x3\x4\x5\x6\x7\x8\x9\xa\xb\xc\xd\xe\xf", 15);
    config.discard();


    /*test(config);
    config.dump(Serial);
*/
    config.setString(_H(Configuration_t().string1), "bla2");
    ConfigSub2_t s = { 1, 0 , 1 };
    config.set(_H(Configuration_t().sub2), s);

    config.setBinary(_H(Configuration_t().blob), "\x1\x2\x3\x4\x5\x6\x7\x8\x9\xa\xb\xc\xd\xe\xf", 15);
    config.dump(Serial);
    config.release();
    config.dump(Serial);

    char *writeableStr = config.getWriteableString(_H(Configuration_t().sub1.string1), sizeof Configuration_t().sub1.string1);
    strcpy(writeableStr, "dummy");

    auto &int32 = config.getWriteable<int32_t>(_H(Configuration_t().int32));
    int32 = -12345678;

    // _H_W_GET_IP is uint32_t
    auto &ip = config._H_W_GET_IP(Configuration_t().ip);
    ip = (uint32_t)IPAddress(1, 2, 3, 4);

    auto test = config._H_GET(Configuration_t().int32);
    printf("%d\n", test);

    // type mismatch and will fail if IPAddress isn't the same type as _H_W_GET_IP, but in this case it is both uint32_t
    IPAddress ip2 = config._H_GET_IP(Configuration_t().ip);
    printf("%s\n", ip2.toString().c_str());

    config._H_SET(Configuration_t().val1, 0x80);
    config._H_SET(Configuration_t().val2, 0x8070);
    config._H_SET(Configuration_t().val3, 0x80706050);
    config._H_SET(Configuration_t().val4, 0x8070605040302010);

    //config.dump(Serial);

    if (config.isDirty()) {
        config.write();
    }

    //config.dump(Serial);
    DumpBinary dump(Serial);

    uint16_t len;
    auto ptr2 = config.getBinary(_H(Configuration_t().blob), len);

    dump.dump(ptr2, len);
    config.release();
    ptr2 = config.getBinary(_H(Configuration_t().blob), len);
    dump.dump(ptr2, len);

    //config.exportAsJson(Serial, "KFCFW 1.x");

    config.release();

    {
        auto& int32 = config.getWriteable<int32_t>(_H(Configuration_t().int32));
        int32++;
    }

    config.dump(Serial);

    config.clear();
    return 0;
#endif
}
