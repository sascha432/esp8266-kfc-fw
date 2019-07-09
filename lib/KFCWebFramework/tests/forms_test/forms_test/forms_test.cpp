
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

typedef uint32_t ConfigFlags_t;

#define FLAGS_SYSLOG_UDP            0x00000200
#define FLAGS_SYSLOG_TCP            0x00000400
#define FLAGS_SYSLOG_TCP_TLS        (FLAGS_SYSLOG_UDP|FLAGS_SYSLOG_TCP)
#define FLAGS_MQTT_ENABLED          0x00000040
#define FLAGS_SECURE_MQTT           0x00008000


class TestForm : public Form {
public:
    TestForm(FormData *data) : Form(data) {
    }

    void createForm() {

        add<WiFiMode_t>(F("mode"), wifi_mode, [this](WiFiMode_t value, FormField *) {
            wifi_mode = value;
        });
        addValidator(new FormRangeValidator(F("Invalid mode"), WIFI_OFF, WIFI_AP_STA));

        // string with length validation
        add<sizeof wifi_ssid>(F("wifi_ssid"), wifi_ssid);
        addValidator(new FormLengthValidator(1, sizeof(wifi_ssid) - 1));

        // select input with range
        add<uint8_t>(F("channel"), &soft_ap_channel);
        addValidator(new FormRangeValidator(1, 13));

        // select input with enumeration validator
        add<uint8_t>(F("encryption"), &encryption);
        std::array<uint8_t, 5> my_array2{ENC_TYPE_NONE, ENC_TYPE_TKIP, ENC_TYPE_WEP, ENC_TYPE_CCMP, ENC_TYPE_AUTO};
        addValidator(new FormEnumValidator<uint8_t, 5>(F("Invalid encryption"), my_array2));

        // check box input with callback to assign/modify/validate the new data
        add<bool>(F("ap_hidden"), is_hidden_ssid, [this](bool value, FormField *) {
            is_hidden_ssid = value ? 123 : 0;
        }, FormField::INPUT_CHECK);

        // select input
        // bits in the array are addressed "0" to "sizeof(array) - 1" inside the form
        // 0 = FormBitValue_UNSET_ALL (none set), 1 = FLAGS_SYSLOG_UDP, 2 = FLAGS_SYSLOG_TCP, etc...
        std::array<ConfigFlags_t, 4> my_array3{ FormBitValue_UNSET_ALL, FLAGS_SYSLOG_UDP, FLAGS_SYSLOG_TCP, FLAGS_SYSLOG_TCP_TLS };
        add<ConfigFlags_t, 4>(F("syslog_enabled"), &flags, my_array3);

        // also a bit set, using the same variable. bits in the array must not overlap
        std::array<ConfigFlags_t, 3> my_array4{ FormBitValue_UNSET_ALL, FLAGS_MQTT_ENABLED, FLAGS_MQTT_ENABLED|FLAGS_SECURE_MQTT };
        add<ConfigFlags_t, 3>(F("mqtt_enabled"), &flags, my_array4);

        // input text
        add<sizeof mqtt_host>(F("mqtt_host"), mqtt_host);
        addValidator(new FormValidHostOrIpValidator());

        add(new FormObject<IPAddress&>(F("dns1"), dns1, FormField::INPUT_TEXT));
        add(new FormObject<IPAddress>(F("dns2"), dns2, [this](const IPAddress &addr, FormField *) {
            dns2 = addr;
        }));

        finalize();
    }

    uint32_t flags;
    WiFiMode_t wifi_mode;
    uint8_t soft_ap_channel;
    uint8_t encryption;
    uint8_t is_hidden_ssid;
    char wifi_ssid[32];
    char mqtt_host[128];
    IPAddress dns1;
    IPAddress dns2;
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
    data.set("ap_hidden", "1");
    data.set("syslog_enabled", "2"); // FLAGS_SYSLOG_TCP
    data.set("mqtt_enabled", "1");  // FLAGS_MQTT_ENABLED
    data.set("mqtt_host", "google.de");
    data.set("dns1", "8.8.8.8");
    data.set("dns2", "8.8.4.4");

    // creating form object with submitted data
    TestForm f1(&data);

    // assign values
    f1.wifi_mode = WIFI_OFF;
    f1.flags = FLAGS_SYSLOG_TCP_TLS|FLAGS_MQTT_ENABLED|FLAGS_SECURE_MQTT;
    f1.soft_ap_channel = 7;
    f1.encryption = 2;
    f1.is_hidden_ssid = true;
    strcpy(f1.wifi_ssid, "OLDSSID");
    strcpy(f1.mqtt_host, "google.com");
    f1.dns1 = IPAddress(4, 4, 4, 1);
    f1.dns2 = IPAddress(4, 3, 2, 1);

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

    assert(String(f1.process("AP_HIDDEN")).indexOf("checked") != -1);

    assert(String(f1.process("SYSLOG_ENABLED")) == "3");
    assert(String(f1.process("MQTT_ENABLED")) == "2");

    assert(String(f1.process("DNS1")) == "4.4.4.1");
    assert(String(f1.process("DNS2")) == "4.3.2.1");

    // submit user data to form without errors using the validate method

    assert(f1.validate());
    assert(f1.isValid());
    assert(f1.flags == (FLAGS_SYSLOG_TCP|FLAGS_MQTT_ENABLED));
    assert(f1.soft_ap_channel == 5);
    assert(strcmp(f1.wifi_ssid, "MYSSID") == 0);
    assert(f1.encryption == 8);
    assert(f1.is_hidden_ssid == 123);
    assert(f1.dns1.toString() == "8.8.8.8");
    assert(f1.dns2.toString() == "8.8.4.4");

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
