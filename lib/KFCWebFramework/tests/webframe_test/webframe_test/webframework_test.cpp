
#include <Arduino_compat.h>
#include <array>
#include <assert.h>
#include "KFCForms.h"

typedef enum WiFiMode 
{
    WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
} WiFiMode_t;

enum wl_enc_type { 
    ENC_TYPE_WEP  = 5,
    ENC_TYPE_TKIP = 2,
    ENC_TYPE_CCMP = 4,
    ENC_TYPE_NONE = 7,
    ENC_TYPE_AUTO = 8
};

class TestForm : public Form {
public:
    TestForm(FormData *data) : Form(data) {
    }

    void createForm() {

        std::array<uint32_t, 4> my_array{ 0, WIFI_STA, WIFI_AP, WIFI_AP_STA };
        add<uint32_t, 4>(F("mode"), &flags, my_array);
        addValidator(new FormRangeValidator(F("Invalid mode"), WIFI_OFF, WIFI_AP_STA));

        add<sizeof wifi_ssid>(F("wifi_ssid"), wifi_ssid);
        addValidator(new FormLengthValidator(1, sizeof(wifi_ssid) - 1));

        add<uint8_t>(F("channel"), &soft_ap_channel);
        addValidator(new FormRangeValidator(1, 13));

        add<uint8_t>(F("encryption"), &encryption);
        std::array<uint8_t, 5> my_array2{ENC_TYPE_NONE, ENC_TYPE_TKIP, ENC_TYPE_WEP, ENC_TYPE_CCMP, ENC_TYPE_AUTO};
        addValidator(new FormEnumValidator<uint8_t, 5>(F("Invalid encryption"), my_array2));

        finalize();
    }

    uint32_t flags;
    uint8_t soft_ap_channel;
    uint8_t encryption;
    char wifi_ssid[32];
};


int main()
{
    FormData data;

    // user submitted data
    data.clear();
    data.set("mode", "1");
    data.set("wifi_ssid", "MYSSID");
    data.set("channel", "5");
    data.set("encryption", "8");

    // creating form object with submitted data
    TestForm f1(&data);

    // assign values
    f1.flags = 0;
    f1.soft_ap_channel = 7;
    f1.encryption = 2;
    strcpy(f1.wifi_ssid, "OLDSSID");

    // create form with assigned values
    f1.createForm(); 

    // verify the converted data of the form

    assert(String(f1.process("MODE")) == "0");

    assert(String(f1.process("CHANNEL")) == "7"); // testing value
    assert(String(f1.process("CHANNEL_6")) == ""); // testing unselected value
    assert(String(f1.process("CHANNEL_7")).indexOf("selected") != -1); // testing selected value
    assert(String(f1.process("CHANNEL_8")) == ""); // testing unselected value

    assert(String(f1.process("WIFI_SSID")) == "OLDSSID");
    assert(String(f1.process("ENCRYPTION")) == "2");

    // submit user data to form without errors using the validate method

    assert(f1.validate());
    assert(f1.isValid());
    assert(f1.flags == 1);
    assert(f1.soft_ap_channel == 5);
    assert(strcmp(f1.wifi_ssid, "MYSSID") == 0);
    assert(f1.encryption == 8);

    // submit invalid data

    data.set("channel", "100");
    data.set("encryption", "101");
    data.set("wifi_ssid", "1234567890123456789012345678901234567890");
    assert(f1.validate() == false);
    assert(f1.isValid() == false);
    assert(f1.hasError(f1.getField("channel")));
    assert(f1.hasError(f1.getField("encryption")));
    assert(f1.hasError(f1.getField("wifi_ssid")));
    assert(f1.getErrors().size() == 3);

    // dump errors
    for (auto error : f1.getErrors()) {
        printf("%s = %s\n", error.getName().c_str(), error.getMessage().c_str());        
    }

    f1.clearErrors();
    assert(f1.getErrors().size() == 0);
}
