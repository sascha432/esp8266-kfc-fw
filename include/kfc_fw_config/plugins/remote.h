/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // Remote

        namespace RemoteControlConfigNS {

            #ifndef IOT_REMOTE_CONTROL_BUTTON_COUNT
            #define IOT_REMOTE_CONTROL_BUTTON_COUNT 4
            #endif

            class RemoteControlConfig {
            public:

                enum class ActionProtocolType : uint8_t {
                    NONE = 0,
                    MQTT,
                    REST,
                    TCP,
                    UDP,
                    MAX
                };

                using ActionIdType = uint16_t;

                #define MAX_ACTION_BITS 4
                #define MAX_ACTION_VAL ((1 << MAX_ACTION_BITS) - 1)

                enum class EventNameType : uint16_t {
                    BUTTON_DOWN = 0,
                    BUTTON_UP,
                    BUTTON_PRESS,
                    BUTTON_LONG_PRESS,
                    BUTTON_SINGLE_CLICK,
                    BUTTON_DOUBLE_CLICK,
                    BUTTON_MULTI_CLICK,
                    BUTTON_HOLD_REPEAT,
                    BUTTON_HOLD_RELEASE,
                    MAX,
                    BV_NONE = 0,
                    BV_BUTTON_DOWN = _BV(BUTTON_DOWN),
                    BV_BUTTON_UP = _BV(BUTTON_UP),
                    BV_BUTTON_PRESS = _BV(BUTTON_PRESS),
                    BV_BUTTON_LONG_PRESS = _BV(BUTTON_LONG_PRESS),
                    BV_BUTTON_SINGLE_CLICK = _BV(BUTTON_SINGLE_CLICK),
                    BV_BUTTON_DOUBLE_CLICK = _BV(BUTTON_DOUBLE_CLICK),
                    BV_BUTTON_MULTI_CLICK = _BV(BUTTON_MULTI_CLICK),
                    BV_BUTTON_HOLD_REPEAT = _BV(BUTTON_HOLD_REPEAT),
                    BV_BUTTON_HOLD_RELEASE = _BV(BUTTON_HOLD_RELEASE),
                    _BV_DEFAULT = BV_BUTTON_PRESS|BV_BUTTON_LONG_PRESS|BV_BUTTON_HOLD_REPEAT,
                    ANY = 0xffff,
                };

                struct __attribute__packed__ Events {
                    using Type = Events;

                    static constexpr size_t kBitCount = 9;

                    union __attribute__packed__ {
                        uint16_t event_bits: kBitCount;
                        struct __attribute__packed__ {
                            uint16_t event_down: 1;
                            uint16_t event_up: 1;
                            uint16_t event_press: 1;
                            uint16_t event_long_press: 1;
                            uint16_t event_single_click: 1;
                            uint16_t event_double_click: 1;
                            uint16_t event_multi_click: 1;
                            uint16_t event_hold: 1;
                            uint16_t event_hold_released: 1;
                        };
                    };

                    Events(EventNameType bits = EventNameType::_BV_DEFAULT) : event_bits(static_cast<uint16_t>(bits)) {}
                    Events(uint16_t bits) : event_bits(bits) {}

                    bool operator[](int index) const {
                        return event_bits & _BV(index);
                    }

                    bool operator[](EventNameType index) const {
                        return event_bits & _BV(static_cast<uint8_t>(index));
                    }

                    CREATE_GETTER_SETTER_TYPE(event_down, 1, uint16_t, bits);
                    CREATE_GETTER_SETTER_TYPE(event_up, 1, uint16_t, bits);
                    CREATE_GETTER_SETTER_TYPE(event_press, 1, uint16_t, bits);
                    CREATE_GETTER_SETTER_TYPE(event_single_click, 1, uint16_t, bits);
                    CREATE_GETTER_SETTER_TYPE(event_double_click, 1, uint16_t, bits);
                    CREATE_GETTER_SETTER_TYPE(event_multi_click, 1, uint16_t, bits);
                    CREATE_GETTER_SETTER_TYPE(event_long_press, 1, uint16_t, bits);
                    CREATE_GETTER_SETTER_TYPE(event_hold, 1, uint16_t, bits);
                    CREATE_GETTER_SETTER_TYPE(event_hold_released, 1, uint16_t, bits);
                };

                typedef struct __attribute__packed__ ComboAction_t {
                    using Type = ComboAction_t;
                    CREATE_UINT16_BITFIELD_MIN_MAX(time, 13, 0, 8000, 1000, 500);
                    CREATE_UINT16_BITFIELD_MIN_MAX(btn0, 2, 0, 3, 0, 1);
                    CREATE_UINT16_BITFIELD_MIN_MAX(btn1, 2, 0, 3, 0, 1);
                } ComboAction_t;

                typedef struct __attribute__packed__ MultiClick_t {
                    using Type = MultiClick_t;
                    CREATE_UINT16_BITFIELD_MIN_MAX(action, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(clicks, 4, 3, 15, 0);
                } MultiClick_t;

                typedef struct __attribute__packed__ Action_t {
                    using Type = Action_t;
                    CREATE_UINT16_BITFIELD_MIN_MAX(down, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(up, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(press, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(long_press, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(single_click, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(double_click, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(multi_click, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(hold, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(hold_released, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);

                    Events udp;
                    Events mqtt;
                    // MultiClick_t multi_click[4];

                    bool hasAction() const {
                        return down || up || press || single_click || double_click || multi_click || long_press || hold || hold_released;
                    }

                    bool hasUdpAction() const {
                        return udp.event_bits != 0;
                    }

                    bool hasMQTTAction() const {
                        return mqtt.event_bits != 0;
                    }

                    #undef MAX_ACTION_BITS
                    #undef MAX_ACTION_VAL

                } Action_t;

                // struct __attribute__packed__ Button_t {
                //     using Type = Button_t;

                //     CREATE_UINT32_BITFIELD_MIN_MAX(multi_click_enabled, 1, 0, 1, 0, 1);
                //     CREATE_UINT32_BITFIELD_MIN_MAX(udp_enabled, 1, 0, 1, 0, 1);
                //     CREATE_UINT32_BITFIELD_MIN_MAX(press_time, 11, 0, 2000, 350, 50);           // millis
                //     CREATE_UINT32_BITFIELD_MIN_MAX(hold_time, 13, 0, 8000, 1000, 50);           // millis
                //     CREATE_UINT32_BITFIELD_MIN_MAX(hold_repeat_time, 12, 0, 4000, 200, 50);     // millis
                //     CREATE_UINT32_BITFIELD_MIN_MAX(hold_max_value, 16, 0, 65535, 255, 1);
                //     CREATE_UINT32_BITFIELD_MIN_MAX(hold_step, 16, 0, 65535, 1, 1);
                //     CREATE_UINT32_BITFIELD_MIN_MAX(action_id, 16, 0, 65535, 0, 1);

                //     Button_t() :
                //         multi_click_enabled(kDefaultValueFor_multi_click_enabled),
                //         udp_enabled(kDefaultValueFor_udp_enabled),
                //         press_time(kDefaultValueFor_press_time),
                //         hold_time(kDefaultValueFor_hold_time),
                //         hold_repeat_time(kDefaultValueFor_hold_repeat_time),
                //         hold_max_value(kDefaultValueFor_hold_max_value),
                //         hold_step(kDefaultValueFor_hold_step),
                //         action_id(kDefaultValueFor_action_id)
                //     {}

                // };

                // struct __attribute__packed__ Event_t {
                //     using Type = Event_t;

                //     CREATE_UINT8_BITFIELD_MIN_MAX(enabled, 1, 0, 1, 0, 1);

                //     Event_t() = default;
                //     Event_t(bool aEnabled = false) : enabled(aEnabled) {}
                // };

                typedef struct __attribute__packed__ Config_t {
                    using Type = Config_t;
                    CREATE_UINT32_BITFIELD_MIN_MAX(auto_sleep_time, 16, 0, 3600 * 16, 2, 1);            // seconds / max. 16 hours
                    CREATE_UINT32_BITFIELD_MIN_MAX(deep_sleep_time, 16, 0, 44640, 0, 1);                // in minutes / max. 31 days, 0 = indefinitely
                    CREATE_UINT32_BITFIELD_MIN_MAX(click_time, 11, 0, 2000, 350, 1);                    // millis
                    CREATE_UINT32_BITFIELD_MIN_MAX(hold_time, 13, 0, 8000, 750, 1);                     // millis
                    CREATE_UINT32_BITFIELD_MIN_MAX(hold_repeat_time, 12, 0, 4000, 500, 1);              // millis
                    CREATE_UINT32_BITFIELD_MIN_MAX(udp_port, 16, 0, 65535, 7881, 1);                    // port
                    CREATE_UINT32_BITFIELD_MIN_MAX(udp_enable, 1, 0, 1, 1, 1);                          // flag
                    CREATE_UINT32_BITFIELD_MIN_MAX(mqtt_enable, 1, 0, 1, 1, 1);                         // flag
                    CREATE_UINT32_BITFIELD_MIN_MAX(max_awake_time, 10, 5, 900, 30, 5);                  // minutes

                    Action_t actions[IOT_REMOTE_CONTROL_BUTTON_COUNT];
                    ComboAction_t combo[4];

                    // Button_t buttons[IOT_REMOTE_CONTROL_BUTTON_COUNT];
                    Events enabled;
                    // Event_t events[9];

                    Config_t();

                } Config_t;

            };

            class RemoteControl : public RemoteControlConfig, public KFCConfigurationClasses::ConfigGetterSetter<RemoteControlConfig::Config_t, _H(MainConfig().plugins.remote.cfg) CIF_DEBUG(, &handleNameRemoteConfig_t)> {
            public:
                static void defaults();
                RemoteControl() {}

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.udp_host, UdpHost, 0, 64);

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name1, Name1, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name2, Name2, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name3, Name3, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name4, Name4, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name5, Name5, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name6, Name6, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name7, Name7, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name8, Name8, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name9, Name9, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name10, Name10, 1, 64);

                static void setName(uint8_t num, const char *);
                static const char *getName(uint8_t num);
                static constexpr uint8_t kButtonCount = IOT_REMOTE_CONTROL_BUTTON_COUNT;

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event1, Event1, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event2, Event2, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event3, Event3, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event4, Event4, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event5, Event5, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event6, Event6, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event7, Event7, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event8, Event8, 1, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event9, Event9, 1, 64);

                static void setEventName(uint8_t num, const char *);
                static const char *getEventName(uint8_t num);
                static constexpr auto kEventCount = static_cast<uint8_t>(EventNameType::MAX);

            };

        }
    }
}
