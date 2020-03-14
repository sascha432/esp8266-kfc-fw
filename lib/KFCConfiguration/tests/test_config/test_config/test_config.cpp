// test_config.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Arduino_compat.h>
#include <Buffer.h>
#include <FS.h>
#include <IntelHexFormat.h>
#include <algorithm>
#include <map>
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

// structure is used for code completion only
typedef struct {

    char string1[100];
    char blob[10];
    int32_t int32;
    uint8_t val1;
    uint16_t val2;
    uint32_t val3;
    uint64_t val4;
    uint32_t val5;
    IPAddress ip;
    struct {
        char string1[100];
        char string2[100];
    } sub1;
    ConfigSub2_t sub2;
    blob_t blob2;
    ConfigFlags flags;
    
} Configuration_t;


#include <assert.h>

#include "push_pack.h"

struct __attribute__packed__  {
    uint32_t _modificationTime;
    uint32_t _fileSize: 24;
    uint32_t _gzipped : 1;
    uint32_t ___reserved : 7;
} x;

int main() {

    ESP._enableMSVCMemdebug();
    DebugHelper::activate();

    File file = SPIFFS.open("C:\\Users\\sascha\\Documents\\PlatformIO\\Projects\\kfc_fw\\data\\webui\\1a22101e.lnk", "r");
    auto l = file.readBytes(reinterpret_cast<char *>(&x), sizeof(x));

    Serial.printf("%u\n", x._fileSize);

    int k = 0;
    return 0;

#if 0
    using milliseconds = std::chrono::duration<uint32_t, std::milli>;
    using seconds = std::chrono::duration<uint32_t, std::ratio<1>>;
    using namespace std::chrono_literals;

    [](milliseconds time) {
        Serial.println(time.count());
    }(seconds(1));

    return 0;
#endif

#if 0

    class Data_t {
    public:
        Data_t() = delete;


        //Data_t() {
        //    num = 0;
        //    id = 0;
        //    Serial.printf("new Data_t()\n");
        //}
        Data_t(std::nullptr_t ptr, int _num2 = 0) {
            static uint32_t n = 0;
            n++;
            //Serial.printf("new Data_t() = %d copy %d\n", num, num2);
            num = n;
            num2 = 0;
            id = num / 3;
        }
        Data_t(uint32_t _num) : num(_num), num2() {
            id = num / 3;
        }
        ~Data_t() {
            //Serial.printf("delete Data_t() = %d copy %d\n", num, num2);
        }
        Data_t(const Data_t &data) {
            *this = data;
        }
        Data_t(Data_t &&data) {
            *this = std::move(data);
        }
        Data_t &operator=(const Data_t &data) {
            num = data.num;
            num2 = data.num2 + 1;
            //Serial.printf("copy Data_t() = %d copy %d\n", num, num2);
            id = data.id;
            return *this;
        }
        Data_t &operator=(Data_t &&data) {
            num = data.num;
            num2 = data.num2;
            id = data.id;
            data.num = -1;
            data.num2 = 0;
            data.id = 0;
            return *this;
        }
        bool operator==(int _num) const {
            return num == _num;
        }
        void print() {
            Serial.printf("Data_t() = %d copy %d\n", num, num2);
        }
        void cprint() const {
            Serial.printf("Data_t() = %d copy %d\n", num, num2);
        }
        struct __attribute__packed__ {
            uint32_t num;
            uint32_t num2;
            uint16_t id;
        };
    };

#if 0
    start_out = true;
    // Data_t()=10, 32x = 320byte data
    // std::stack, peak mem. 446, realloc 2x, mem=446
    auto lptr = new std::stack<Data_t>;
    auto &l = *lptr;
    for (int i = 0; i < 31; i++) {
        l.push(Data_t(nullptr));
    }
    start_out = false;
    return 0;
#endif

#if 0
    start_out = true;
    // Data_t()=10, 32x = 320byte data
    // std::deque, peak mem. 446, realloc 2x, mem=446
    auto lptr = new std::deque<Data_t>;
    auto &l = *lptr;
    for (int i = 0; i < 31; i++) {
        l.emplace_back(nullptr);
    }
    start_out = false;
    return 0;
#endif

#if 0
    start_out = true;
    // Data_t()=10, 32x = 320byte data
    // std::forward_list, peak mem. 512, realloc 0x, mem=512
    auto lptr = new std::forward_list<Data_t>;
    auto &l = *lptr;
    for (int i = 0; i < 31; i++) {
        l.push_front(Data_t(nullptr));
    }
    start_out = false;
    return 0;
#endif

#if 0
    start_out = true;
    // Data_t()=10, 32x = 320byte data
    // std::list, peak mem. 660, realloc 0x, mem=660
    auto lptr = new std::list<Data_t>;
    auto &l = *lptr;
    for (int i = 0; i < 31; i++) {
        l.push_back(Data_t(nullptr));
    }
    start_out = false;
    return 0;
#endif

#if 0
    start_out = true;
    // Data_t()=10, 32x = 320byte data
    // std::vector, peak mem. 724, realloc 9x, mem=444
    auto lptr = new std::vector<Data_t>;
    auto &l = *lptr;
    for (int i = 0; i < 31; i++) {
        l.push_back(Data_t(nullptr));
    }
    start_out = false;
    return 0;
#endif


#if 0
    xtra_containers::chunk_list_size<Data_t>::list(Serial, 1, 64, 16);
    start_out = true;
    // Data_t()=10, 32x = 320byte data
    // xtra_containers::chunked_list, chunks=1, peak mem. 444, realloc 0x, mem=444, capacity=0
    // xtra_containers::chunked_list, chunks=2, peak mem. 394, realloc 0x, mem=394, capacity=1
    // xtra_containers::chunked_list, chunks=3, peak mem. 384, realloc 0x, mem=384, capacity=2
    // xtra_containers::chunked_list, chunks=4, peak mem. 362, realloc 0x, mem=362, capacity=1
    // xtra_containers::chunked_list, chunks=6, peak mem. 394, realloc 0x, mem=394, capacity=5, *16 byte memory alignment
    // xtra_containers::chunked_list, chunks=8, peak mem. 346, realloc 0x, mem=346, capacity=1
    // xtra_containers::chunked_list, chunks=14, peak mem. 442, realloc 0x, mem=442, capacity=11, *16 byte memory alignment
    auto lptr = new xtra_containers::chunked_list<Data_t, 4>;
    auto &l = *lptr;
    for (int i = 0; i < 31; i++) {
        l.push_back(Data_t(nullptr));
    }
    Serial.println(l.capacity());
    start_out = false;
    return 0;
#endif
    typedef xtra_containers::chunked_list<Data_t, 6> MyList;
    MyList _params;

    class testx {
    public:
        testx(MyList &l) : _l(l) {
        }

        void clist() const {
            Serial.println("---");
            for (const auto &param : _l) {
                param.cprint();
            }
        }
        const MyList &_l;
    };

    auto value = std::distance(_params.begin(), _params.end());
    assert(value == 0);

    MyList _params5;
    for (int i = 0; i < 57; i++) {
        _params.emplace_back(nullptr, i);

        assert(_params.front().num == 1);
        assert(_params.back().num == i + 1);

#if 1
        _params5 = _params;
        assert(_params.front().num == _params5.front().num);
        assert(_params.back().num == _params5.back().num);
        assert(_params.front().num2 + 1 == _params5.front().num2);
        assert(_params.back().num2 + 1 == _params5.back().num2);
        assert(_params.capacity() == _params5.capacity());
        assert(_params.size() == _params5.size());
        assert(std::distance(_params.begin(), _params.end()) == std::distance(_params5.begin(), _params5.end()));
        assert(std::distance(_params.begin(), _params.end()) == _params.size());
#endif
    }

    auto oldSize = _params.size();
    MyList _params8;
    std::swap(_params8, _params);
    _params8.swap(_params);
    MyList _params7 = _params;
    assert(oldSize == _params.size());

    auto iterator = std::find(_params7.begin(), _params7.end(), 5);
    assert(iterator != _params7.end());
    assert(iterator->num == 5);

    iterator = std::find_if(_params7.begin(), _params7.end(), [](const Data_t &data) {
        return data.num == 8;
        });
    assert(iterator != _params7.end());
    assert(iterator->num == 8);

#if 0
    testx(_params).clist();
    testx(_params7).clist();
    _params7.swap(_params);
    testx(_params).clist();
    testx(_params7).clist();
#endif

    //MyList _params2;
    //_params2 = _params;
    //MyList _params3 = std::move(_params);
    //Serial.printf("%u %u %u", _params.size(), _params2.size(), _params3.size());


    //testx(_params).clist();
    //testx(_params2).clist();
    //testx(_params3).clist();



    _params.clear();

    /*    auto it = _params.begin();
        ++it;++it;++it;++it;
        auto f = it;
        ++it;++it;++it;++it;++it;++it;++it;
        Serial.printf("%d %d\n", std::distance(f, it), _params.size());
        Serial.printf("%d %d\n", std::distance(f, f), _params.size());
        Serial.printf("%d %d\n", std::distance(it, it), _params.size());
        *///Serial.printf("%d %d\n", std::distance(it, f), _params.size());

        //_params.push_back(Data_t(nullptr));
        //_params.push_back(Data_t(nullptr));
        //_params.emplace_back(nullptr);



        //auto it = _params.begin();
        //++it;
        //++it;
        //++it;
        //*it = Data_t(666);

        //auto result = _params.end() == _params.end();
        //auto it = _params.begin();
        //++it;++it;++it;++it;++it;++it;++it;++it;++it;
        //it->print();
        //++it;
        //result = it == _params.end();

    Serial.printf("--- %u\n", _params.size());
    for (auto iterator = _params.begin(); iterator != _params.end(); ++iterator) {
        if (iterator->num == 666) {
            //_params.push_back(Data_t(nullptr));
            //_params.push_back(Data_t(nullptr));
        }
        iterator->print();
        //iterator.get()->print();
    }

    Serial.printf("--- %u\n", _params.size());
    for (auto iterator = _params.cbegin(); iterator != _params.cend(); ++iterator) {
        //iterator.get()->cprint();
        //iterator->cprint();
        (*iterator).cprint();
    }

    return 0;

#endif

#if 0
    IntelHexFormat _file;

    fpout = fopen("out.hex", "w");

    if (_file.open("C:\\Users\\sascha\\Documents\\PlatformIO\\Projects\\kfc_fw\\data\\firmware.hex")) {

        if (_file.validate()) {

            int total = 0;

            while (!_file.isEOF()) {
                bool exit = false;
                do {
                    char buffer[1];
                    uint16_t address;
                    uint16_t read = sizeof(buffer);
                    if (_pagePosition != _pageSize && read > _pageSize - _pagePosition) { // limit length to space left in page buffer
                        read = _pageSize - _pagePosition;
                    }
                    auto length = _file.readBytes(buffer, read, address);
                    if (length == -1) {
                        // ERROR
                        printf("length=-1\n");
                    }
                    else if (length == 0) {
                        // ERROR
                        printf("length=0, address %u, eof  %u\n", address, _file.isEOF());
                    }
                    total += length;

                    uint16_t pageAddress = (address >> 7);
                    if (_pageAddress != pageAddress || length == 0) {  // new page
                        // write buffer and clear buffer
                        _writePage((_pageAddress << 6), _pagePosition);

                        _clearPageBuffer();
                        _pageAddress = pageAddress;
                        _pagePosition = 0;

                        // leave loop
                        exit = true;
                    }
                    printf("%u %u\n", total, written);

                    uint16_t pageOffset = address - (_pageAddress << 7);
                    memcpy(_pageBuffer + pageOffset, buffer, length);
                    _pagePosition = pageOffset + length;

                    if (_file.isEOF()) {
                        break;
                    }

                } while (!exit);

                // wait for response
            }

            if (_file.isEOF() && _pagePosition) {
                _writePage((_pageAddress << 6), _pagePosition);
                _pagePosition = 0;
            }


            //uint16_t address;
            //char buf[128];
            //int result = -1;

            //while(!_file.isEOF() && ((result = _file.readBytes(buf, sizeof(buf), address)) > 0)) {
            //    printf("%04x: ", address & 0xffff);
            //    auto ptr = buf;
            //    uint8_t count = result;
            //    while(count--) {
            //        printf("%02x ", *ptr++ & 0xff);
            //    }
            //    printf("\n");
            //}

            //if (result == -1) {
            //    printf("Error: %s\n", _file.getErrorMessage().c_str());
            //}
        }
        else {
            printf("Verification failed\n");
        }
    }
    else {
        printf("Cannot open file\n");
    }
    return 0;

    //test_tokenizer();
    //test_stringlistfind();
    //test_tabledumper();
#endif

#if 0
    memcpy(EEPROM.getDataPtr(), tmp, sizeof(tmp));
    EEPROM.commit();
    Configuration config2(28, 4096);
    if (!config2.read()) {
        printf("Failed to read configuration/n");
    }
    config2.dump(Serial);
    return 0;
#endif

#if 0
    EEPROM.clear();
#endif
    EEPROM.begin();
    //EEPROM.clear();

    Configuration config(512, 2048);
    if (!config.read()) {
        printf("Failed to read configuration\n");
    }
    config.dump(Serial);

    auto flags = config._H_GET(Configuration_t().flags);
    //flags = ConfigFlags();
  /*  flags.counter++;
    flags.atModeEnabled = true;
    flags.isDefaultPassword = true;
    flags.stationModeDHCPEnabled = true;
    flags.wifiMode = 3;
    flags.webServerMode = 1;*/


    //"2702C001"
    //"3F03C001"
    //"BF03C001"
    //"BF0B C001"
    //"BF8B C201"

    flags.counter++;
    config._H_SET(Configuration_t().flags, flags);

    flags = config._H_GET(Configuration_t().flags);

    flags.counter++;
    config._H_SET(Configuration_t().flags, flags);

    flags = config._H_GET(Configuration_t().flags);

    flags.counter++;
    config._H_SET(Configuration_t().flags, flags);

    //auto mzstring = config._H_STR(Configuration_t().string1);


    //config._H_SET_STR(Configuration_t().string1, emptyString);

    char data[200] = {};

    config.setBinary(_H(binary), data, 0);
    config.setBinary(_H(binary), data, 0);

    config.write();

    return 0;

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

    config.setString(_H(str1new), "");

    //config.setString(_H(str2), "str2_long");
    //config.setString(_H(str3), "str3");
    config.write();


#if 1
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
}
