/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <stl_ext/utility.h>
#include <stl_ext/memory.h>
#include "mqtt_client.h"
#include "component_proxy.h"
#include "auto_discovery_list.h"

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;
using KFCConfigurationClasses::System;

using namespace MQTT::AutoDiscovery;

Queue::Queue(Client &client) :
    Component(ComponentType::AUTO_DISCOVERY),
    _client(client),
    _packetId(0),
    _runFlags(RunFlags::DEFAULTS)
{
    MQTTClient::registerComponent(this);
}

Queue::~Queue()
{
    __LDBG_printf("dtor collect=%p remove=%p timer=%u", _collect.get(), _remove.get(), (bool)_timer);
    clear();

    // if the component is still registered, publish done has not been called
    if (MQTTClient::unregisterComponent(this)) {
        _publishDone(StatusType::FAILURE);
    }

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
    __LDBG_printf("--- callbacks");
    for(const auto &topic: _client.getTopics()) {
        __LDBG_printf("topic=%s component=%p name=%s", topic.getTopic().c_str(), topic.getComponent(), topic.getComponent()->getName());
    }
    __LDBG_printf("--- end callbacks");
#endif
}

// EntityPtr Queue::getAutoDiscovery(FormatType format, uint8_t num)
// {
//     return nullptr;
// }

// uint8_t Queue::getAutoDiscoveryCount() const
// {
//     return 0;
// }

void Queue::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    __LDBG_printf("auto discovery aborted, marked for restart");
    client()._startAutoDiscovery = true;
}

void Queue::onPacketAck(uint16_t packetId, PacketAckType type)
{
    __LDBG_printf("packet=%u<->%u type=%u", _packetId, packetId, type);
    if (_packetId == packetId) {
        __LDBG_printf("packet=%u type=%u", packetId, type);
        _packetId = 0;
        if (type == PacketAckType::TIMEOUT) {
            LoopFunctions::callOnce([this]() {
                _publishDone(StatusType::FAILURE);
            });
            return;
        }
        else {
            _crcs.update(_iterator->getTopic(), _iterator->getPayload());
            ++_iterator;
            LoopFunctions::callOnce([this]() {
                _publishNextMessage();
            });
        }
    }
}


bool Queue::isEnabled(bool force)
{
#if MQTT_AUTO_DISCOVERY
    auto cfg = Plugins::MQTTClient::getConfig();
    return
        System::Flags::getConfig().is_mqtt_enabled && cfg.auto_discovery && (force || (cfg.auto_discovery_delay != 0));
#else
    return false;
#endif
}

void Queue::clear()
{
    __LDBG_printf("clear entities=%u done=%u", _entities.size(), _iterator == _entities.end());
    _timer.remove();
    _crcs.clear();

//TODO crashing on restart

// shutdown plugin=cfg
// shutdown plugin=rd
// scheduled tasks 4, WiFi callbacks 5, Loop Functions 2
// terminating pin monitor
// terminating serial handlers
// clearing wifi callbacks
// clearing loop functions
// terminating event scheduler
// invoking restart

// --------------- CUT HERE FOR EXCEPTION DECODER ---------------

// Exception (9):
// epc1=0x4021b640 epc2=0x00000000 epc3=0x00000000 excvaddr=0x0788078e depc=0x00000000

// LoadStoreAlignmentCause: Load or store to an unaligned address
//   epc1=0x4021b640 in MQTT::AutoDiscovery::List::end() at src\plugins\mqtt/component.h:296
//      (inlined by) ?? at src\plugins\mqtt/component.h:291
//      (inlined by) MQTT::AutoDiscovery::List::end() at src\plugins\mqtt/auto_discovery_list.cpp:26

// >>>stack>>>

// ctx: cont
// sp: 3ffff730 end: 3fffffc0 offset: 0190
// 3ffff8c0:  402b287c 00000008 3ffff8a0 3ffff8d0
// 3ffff8d0:  3ffef6d4 3fff454c 00000000 3ffef700
// 3ffff8e0:  3fff3ee8 3fff3edc 3fff3e9c 4021bc21
// 3ffff8f0:  00000001 402c1627 3fff4550 00000000
// 3ffff900:  3ffef600 34390000 3fff3e9c 40214fbd
// 3ffff910:  3ffff950 3ffff940 00000008 4023f8e7
// 3ffff920:  00000000 00000000 3fff3af4 40215015
// 3ffff930:  3ffff950 3ffff940 00000008 40275308
// 3ffff940:  00007270 3ffe8b52 00002d65 40270c6c
// 3ffff950:  3fff3554 3fff3554 00000001 40275308
// 3ffff960:  000dbba0 00000000 3fff3e9c 4021c268
// 3ffff970:  00000000 3ffef6d4 3fff4550 00000000
// 3ffff980:  40275308 00000000 3fff46cc 0044004f
// 3ffff990:  00ff3e9c 4021b894 000b000f 40270c6c
// 3ffff9a0:  40270c58 00000000 00000000 3fff4550
// 3ffff9b0:  00000003 3fff3a34 3ffef6d4 4021e9fc
// 3ffff9c0:  00000000 3ffe8b52 00002d5b 40214ee1
// 3ffff9d0:  00002738 000004e7 000004e7 3fff4944
// 3ffff9e0:  3fff4944 3fff4944 3fff3e9c 4021c2bb
// 3ffff9f0:  00000000 3fff4944 3fff3e9c 4021c307
// 3ffffa00:  3ffffa20 3ffffa10 3fff3a34 402716a2
// 3ffffa10:  402b8f7c 3fff4b9c 3ffffa48 4021d194
// 3ffffa20:  00000001 3ffff9b9 3fff3a34 3fff4944
// 3ffffa30:  3fff4944 3fff4944 3fff3a34 4021db54
// 3ffffa40:  40275308 00000000 3fff3000 0019001f
// 3ffffa50:  80ff07d8 00000000 00000000 4028b310
// 3ffffa60:  3fff4550 3fff4cc4 00000001 40265d80
// 3ffffa70:  3fff07d8 00000000 3ffffaa0 3fff4b9c
// 3ffffa80:  3fff3284 00000000 3fff3284 4021db85
// 3ffffa90:  402433bc 00000000 3fff3b74 40261489
// 3ffffaa0:  3fff457c 3fff07d8 4021cda8 4021db70
// 3ffffab0:  402433bc 3fff3b74 3fff3b74 402614ac
// 3ffffac0:  3fff4ba0 3fff4b9c 3fff45ec 4010035d
// 3ffffad0:  00000014 00006008 0000dbcc 40243011
// 3ffffae0:  777ca875 0000dbcc fffffff3 402433ed
// 3ffffaf0:  3fff3d24 3fff3d34 3fff4b9c 40281538
// 3ffffb00:  402433bc 00000001 0000dbcc 40281b9a
// 3ffffb10:  0000075b 3fff07dc 00002000 0000a000
// 3ffffb20:  00001c86 777ca875 3fff3b74 00000000
// 3ffffb30:  00000011 3fff0784 00000020 3ffe9858
// 3ffffb40:  3ffe9858 3ffffba0 3fff5f34 40281bb8
// 3ffffb50:  3ffffbd0 3fff59b4 3fff2e94 40281c40
// 3ffffb60:  000050c0 3ffffb80 3ffffba0 40281c6f
// 3ffffb70:  0000000a 3ffffba0 3fff07d8 40285151
// 3ffffb80:  00000000 00000000 00000020 4010157f
// 3ffffb90:  3ffe9858 3ffe9858 3fff07d8 402851d8
// 3ffffba0:  9300a8c0 3fff59b4 00000000 4028b330
// 3ffffbb0:  3fff59b4 3fff07d8 3fff16ec 3fff5b10
// 3ffffbc0:  3fff59b4 3fff07d8 3fff16ec 4028831c
// 3ffffbd0:  0300a8c0 40290009 3ffed450 3ffed450
// 3ffffbe0:  00000005 00000000 00000000 00000001
// 3ffffbf0:  00000000 3ffe9e00 00000018 402b31d0
// 3ffffc00:  3fff0cfc 00000000 3fff07d8 402811c7
// 3ffffc10:  00000000 3fff148c 3ffee7c8 402a74dc
// 3ffffc20:  3fff148c 3fff0cfc 3fff0cfc 40295921
// 3ffffc30:  00000000 00000000 40293569 3fff0cfc
// 3ffffc40:  40293691 3fff0cfc 00000008 3fff0d14
// 3ffffc50:  402b31d0 3fff0cfc 3fff27fc 3fff4584
// 3ffffc60:  402a5c84 3fff0cfc 3fff45cc 3ffefbe4
// 3ffffc70:  3fff4584 3ffee7c8 3ffedf7c 00000000
// 3ffffc80:  40294458 3ffef8e4 3fff45cc 00000003
// 3ffffc90:  402a36c2 3fff3f1c 3fff27f4 4022e9b7
// 3ffffca0:  40262e51 00000003 3ffef8f0 4023c055
// 3ffffcb0:  3ffefbe4 3fff45cc 3ffef8e4 4020f30e
// 3ffffcc0:  3fff315c 0019001f 00fef810 40263db4
// 3ffffcd0:  3fff48f4 3fff45cc 3ffffdc8 40201740
// 3ffffce0:  402b6b94 3ffef601 3ffffdc8 3ffef600
// 3ffffcf0:  00000000 00000000 3ffffdc8 4020af16
// 3ffffd00:  00000000 4bc6a7f0 33f7ced9 00000000
// 3ffffd10:  00000000 00000000 4bc6a7f0 00000000
// 3ffffd20:  4024176c 00000000 40270cc0 40270ca8
// 3ffffd30:  3ffffdb2 401044e6 3ffedd20 40100714
// 3ffffd40:  0031527c 00000000 0000001f 3ffef838
// 3ffffd50:  00000002 3ffffdb0 3fff1dc4 40263748
// 3ffffd60:  00000002 402bd470 3fff1dc4 40240b66
// 3ffffd70:  00000002 00002b0c 3fff1dc8 3ffffdb0
// 3ffffd80:  00000000 00000000 00000020 40100714
// 3ffffd90:  00000000 40104218 00000000 3ffffe60
// 3ffffda0:  00000002 402bd470 00000000 40263d01
// 3ffffdb0:  00000a0d 401044e6 3ffedd20 40105bc9
// 3ffffdc0:  3fff2904 2c9f0300 40274080 3ffef810
// 3ffffdd0:  00545352 3fffc200 83000022 00000000
// 3ffffde0:  00000000 00000000 3fff2a00 40258d3f
// 3ffffdf0:  3ffef65c 00000020 00000501 00000002
// 3ffffe00:  00000001 3ffffe48 00000000 402570d4
// 3ffffe10:  3ffef810 00000000 3fff007d 40105bc9
// 3ffffe20:  4000050c 00000004 3fff27fc 3ffffdd0
// 3ffffe30:  00000002 0000007f 00000010 40209499
// 3ffffe40:  007dfde6 3ffefbf4 00000004 3ffef600
// 3ffffe50:  00000009 3ffef5fd 3fff27fc 4020d377
// 3ffffe60:  40275358 00000000 000003e8 00000030
// 3ffffe70:  3fff172c 3ffef800 3ffef838 00000030
// 3ffffe80:  00000000 4bc6a7f0 007dfde6 00000000
// 3ffffe90:  00000000 00000000 4bc6a7f0 00000000
// 3ffffea0:  402689ba 00000001 3fff1de4 00000002
// 3ffffeb0:  00000000 3ffef82c 3fff27f4 40270c8c
// 3ffffec0:  00315249 00810008 ffffff81 40100291
// 3ffffed0:  00000000 3ffef82c 00000001 4022edee
// 3ffffee0:  3fffff10 3fff2820 3fff27f4 3ffef810
// 3ffffef0:  00000000 00000001 3ffef838 00000000
// 3fffff00:  3ffef810 00000001 3ffef838 4022ee4c
// 3fffff10:  0000000d 3fff3180 3ffef8f0 00000002
// 3fffff20:  3ffef8e4 3ffef6c4 3ffefbf4 4025c499
// 3fffff30:  3ffef82c 00000007 00000030 4025a886
// 3fffff40:  3ffef8e4 3ffef8e4 3ffef65c 40219e65
// 3fffff50:  3ffef8e4 3ffef8e4 3ffef810 4022ef52
// 3fffff60:  3ffef8e4 3ffef8e4 00000000 4022ef74
// 3fffff70:  00000000 3fff2ec4 00000020 4020e5e9
// 3fffff80:  00000000 00000000 00000001 40100714
// 3fffff90:  3fffdad0 00000000 3fff00f0 3fff0130
// 3fffffa0:  3fffdad0 00000000 3fff00f0 4026605c
// 3fffffb0:  feefeffe feefeffe 3ffe85c8 40100679
// <<<stack<<<

// 0x402b287c in sleep_reset_analog_rtcreg_8266 at ??:?
// 0x4021bc21 in MQTT::AutoDiscovery::Queue::clear() at src\plugins\mqtt/auto_discovery_queue.cpp:113 (discriminator 1)
// 0x402c1627 in chip_v6_unset_chanfreq at ??:?
// 0x40214fbd in Logger::writeLog(Logger::Level, String const&, __va_list_tag) at include/logger.h:80
// 0x4023f8e7 in PrintString::write(unsigned char) at lib\KFCBaseLibrary\src/PrintString.cpp:90
// 0x40215015 in Logger::notice(String const&, ...) at src/logger.cpp:108
// 0x40275308 in EEPROMClass::getConstDataPtr() const at ??:?
// 0x40270c6c in std::_Function_base::_Base_manager<void (*)(Event::CallbackTimer*)>::_M_manager(std::_Any_data&, std::_Any_data const&, std::_Manager_operation) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:1934
// 0x40275308 in EEPROMClass::getConstDataPtr() const at ??:?
// 0x4021c268 in MQTT::AutoDiscovery::Queue::_publishDone(MQTT::StatusType, unsigned short) at src\plugins\mqtt/auto_discovery_queue.cpp:396
// 0x40275308 in EEPROMClass::getConstDataPtr() const at ??:?
// 0x4021b894 in MQTT::Client::publishAutoDiscoveryCallback(Event::CallbackTimer*) at src\plugins\mqtt/mqtt_client.h:776
// 0x40270c6c in std::_Function_base::_Base_manager<void (*)(Event::CallbackTimer*)>::_M_manager(std::_Any_data&, std::_Any_data const&, std::_Manager_operation) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:1934
// 0x40270c58 in std::_Function_handler<void (Event::CallbackTimer*), void (*)(Event::CallbackTimer*)>::_M_invoke(std::_Any_data const&, Event::CallbackTimer*) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2069
// 0x4021e9fc in MQTT::Client::unregisterComponent(MQTT::Component*) at src\plugins\mqtt/component.h:224
//  (inlined by) MQTT::Client::unregisterComponent(MQTT::Component*) at src\plugins\mqtt/client.cpp:254
// 0x40214ee1 in Logger::writeLog(Logger::Level, __FlashStringHelper const*, __va_list_tag) at include/logger.h:83
// 0x4021c2bb in MQTT::AutoDiscovery::Queue::~Queue() at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2174
//  (inlined by) MQTT::AutoDiscovery::Queue::~Queue() at src\plugins\mqtt/auto_discovery_queue.cpp:34
// 0x4021c307 in MQTT::AutoDiscovery::Queue::~Queue() at src\plugins\mqtt/auto_discovery_queue.cpp:51
// 0x402716a2 in _ZNKSt14default_deleteIN4MQTT13AutoDiscovery5QueueEEclEPS2_$isra$75 at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2\bits/unique_ptr.h:68
// 0x402b8f7c in chip_v6_unset_chanfreq at ??:?
// 0x4021d194 in MQTT::Client::_resetClient() at src\plugins\mqtt/client.cpp:148
// 0x4021db54 in MQTT::Client::onDisconnect(AsyncMqttClientDisconnectReason) at src\plugins\mqtt/client.cpp:653
// 0x40275308 in EEPROMClass::getConstDataPtr() const at ??:?
// 0x4028b310 in mem_malloc at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/mem.c:210
// 0x40265d80 in operator new(unsigned int) at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/abi.cpp:39
// 0x4021db85 in std::_Function_handler<void (AsyncMqttClientDisconnectReason), MQTT::Client::_setupClient()::{lambda(AsyncMqttClientDisconnectReason)#2}>::_M_invoke(std::_Any_data const&, AsyncMqttClientDisconnectReason) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2073
// 0x402433bc in AsyncClient::_s_error(void*, long) at lib-extra\ESPAsyncTCP\src/ESPAsyncTCP.cpp:741
// 0x40261489 in AsyncMqttClient::_onDisconnect(AsyncClient*) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2174
//  (inlined by) AsyncMqttClient::_onDisconnect(AsyncClient*) at lib-extra\AsyncMqttClient\src/AsyncMqttClient.cpp:369
// 0x4021cda8 in std::_Function_base::_Base_manager<MQTT::Client::_setupClient()::{lambda(AsyncMqttClientDisconnectReason)#2}>::_M_manager(std::_Any_data&, std::_Function_base::_Base_manager<MQTT::Client::_setupClient()::{lambda(AsyncMqttClientDisconnectReason)#2}> const&, std::_Manager_operation) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:1931
// 0x4021db70 in std::_Function_handler<void (AsyncMqttClientDisconnectReason), MQTT::Client::_setupClient()::{lambda(AsyncMqttClientDisconnectReason)#2}>::_M_invoke(std::_Any_data const&, AsyncMqttClientDisconnectReason) at src\plugins\mqtt/mqtt_client.h:791
//  (inlined by) operator() at src\plugins\mqtt/client.cpp:202
//  (inlined by) _M_invoke at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2071
// 0x402433bc in AsyncClient::_s_error(void*, long) at lib-extra\ESPAsyncTCP\src/ESPAsyncTCP.cpp:741
// 0x402614ac in std::_Function_handler<void (void*, AsyncClient*), AsyncMqttClient::AsyncMqttClient()::{lambda(void*, AsyncClient*)#2}>::_M_invoke(std::_Any_data const&, void*, AsyncClient*) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2073
// 0x4010035d in std::function<void (void*, AsyncClient*)>::operator()(void*, AsyncClient*) const at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2465
// 0x40243011 in AsyncClient::_error(long) at lib-extra\ESPAsyncTCP\src/ESPAsyncTCP.cpp:533
// 0x402433ed in AsyncClient::_s_error(void*, long) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2\bits/shared_ptr_base.h:781
//  (inlined by) ?? at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2\bits/shared_ptr.h:93
//  (inlined by) AsyncClient::_s_error(void*, long) at lib-extra\ESPAsyncTCP\src/ESPAsyncTCP.cpp:746
// 0x40281538 in tcp_free at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/tcp.c:217
// 0x402433bc in AsyncClient::_s_error(void*, long) at lib-extra\ESPAsyncTCP\src/ESPAsyncTCP.cpp:741
// 0x40281b9a in tcp_abandon at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/tcp.c:624
// 0x40281bb8 in tcp_abort at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/tcp.c:641
// 0x40281c40 in tcp_netif_ip_addr_changed_pcblist at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/tcp.c:2325
// 0x40281c6f in tcp_netif_ip_addr_changed at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/tcp.c:2342
// 0x40285151 in netif_do_ip_addr_changed at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/netif.c:452
//  (inlined by) netif_do_set_ipaddr at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/netif.c:475
// 0x4010157f in free at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266\umm_malloc/umm_malloc.cpp:398
// 0x402851d8 in netif_set_addr_LWIP2 at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/netif.c:692
// 0x4028b330 in mem_free at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/mem.c:237
// 0x4028831c in dhcp_release_and_stop at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/lwip2-src/src/core/ipv4/dhcp.c:1380
// 0x40290009 in ieee80211_output_pbuf at ??:?
// 0x402b31d0 in umm_block_size at ??:?
// 0x402811c7 in esp2glue_netif_set_up1down0 at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/glue-lwip/lwip-git.c:495
// 0x402a74dc in netif_set_down at /local/users/gauchard/arduino/arduino_esp8266/esp8266-lwip/tools/sdk/lwip2/builder/glue-esp/lwip-esp.c:623
// 0x40295921 in cnx_sta_leave at ??:?
// 0x40293569 in ieee80211_sta_new_state at ??:?
// 0x40293691 in ieee80211_sta_new_state at ??:?
// 0x402b31d0 in umm_block_size at ??:?
// 0x402a5c84 in wifi_station_disconnect at ??:?
// 0x40294458 in wifi_station_stop at ??:?
// 0x402a36c2 in system_restart at ??:?
// 0x4022e9b7 in std::_Function_base::_Base_manager<SerialHandler::Wrapper::begin()::{lambda()#1}>::_M_manager(std::_Any_data&, std::_Function_base::_Base_manager<SerialHandler::Wrapper::begin()::{lambda()#1}> const&, std::_Manager_operation) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:1954
// 0x40262e51 in EspClass::restart() at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/Esp.cpp:204
// 0x4023c055 in Event::Scheduler::end() at lib\KFCEventScheduler\src/Scheduler.cpp:107
// 0x4020f30e in KFCFWConfiguration::restartDevice(bool) at src/kfc_fw_config.cpp:1114
// 0x40263db4 in Print::println(__FlashStringHelper const*) at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/Print.cpp:178
// 0x40201740 in AtModeArgs::print(__FlashStringHelper const*) at src/AtModeArgs.cpp:206
// 0x402b6b94 in chip_v6_unset_chanfreq at ??:?
// 0x4020af16 in at_mode_serial_handle_event(String&) at src/at_mode.cpp:1461 (discriminator 1)
// 0x4024176c in tokenizerCmpFunc(char, int) at lib\KFCBaseLibrary\src/misc.cpp:1051
// 0x40270cc0 in std::_Function_base::_Base_manager<bool (*)(char, int)>::_M_manager(std::_Any_data&, std::_Any_data const&, std::_Manager_operation) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:1934
// 0x40270ca8 in std::_Function_handler<bool (char, int), bool (*)(char, int)>::_M_invoke(std::_Any_data const&, char, int) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2054
// 0x401044e6 in lmacRecycleMPDU at ??:?
// 0x40100714 in ets_post at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/core_esp8266_main.cpp:177
// 0x40263748 in HardwareSerial::write(unsigned char const*, unsigned int) at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/HardwareSerial.h:165
// 0x402bd470 in chip_v6_unset_chanfreq at ??:?
// 0x40240b66 in StreamWrapper::write(unsigned char const*, unsigned int) at lib\KFCBaseLibrary\src/StreamWrapper.cpp:199
// 0x40100714 in ets_post at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/core_esp8266_main.cpp:177
// 0x40104218 in lmacProcessTXStartData at ??:?
// 0x402bd470 in chip_v6_unset_chanfreq at ??:?
// 0x40263d01 in Print::print(__FlashStringHelper const*) at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/Print.cpp:114
// 0x401044e6 in lmacRecycleMPDU at ??:?
// 0x40105bc9 in ets_timer_disarm at ??:?
// 0x40274080 in EEPROMClass::getConstDataPtr() const at ??:?
// 0x40258d3f in ConfigurationParameter::getBinary(Configuration&, unsigned short&, unsigned short) at lib\KFCConfiguration\src/ConfigurationParameter.cpp:102
// 0x402570d4 in Configuration::getBinary(unsigned short, unsigned short&) at lib\KFCConfiguration\src/Configuration.cpp:225
// 0x40105bc9 in ets_timer_disarm at ??:?
// 0x40209499 in KFCConfigurationClasses::ConfigGetterSetter<KFCConfigurationClasses::System::FlagsConfig::ConfigFlags_t, (unsigned short)28432, &handleNameFlagsConfig_t>::getConfig() at include/kfc_fw_config/base.h:289
// 0x4020d377 in at_mode_serial_input_handler(Stream&) at src/at_mode.cpp:2470
// 0x40275358 in EEPROMClass::getConstDataPtr() const at ??:?
// 0x402689ba in uart_read at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/uart.cpp:285
// 0x40270c8c in std::_Function_handler<void (Stream&), void (*)(Stream&)>::_M_invoke(std::_Any_data const&, Stream&) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2073
// 0x40100291 in std::function<void (Stream&)>::operator()(Stream&) const at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2465
// 0x4022edee in SerialHandler::Wrapper::_writeClientsRx(SerialHandler::Client*, unsigned char const*, unsigned int, SerialHandler::EventType) at src/serial_handler.cpp:252
// 0x4022ee4c in SerialHandler::Wrapper::_pollSerial() at src/serial_handler.cpp:284
// 0x4025c499 in esp8266::MDNSImplementation::MDNSResponder::_process(bool) at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\libraries\ESP8266mDNS\src/LEAmDNS_Control.cpp:90
// 0x4025a886 in esp8266::MDNSImplementation::MDNSResponder::update() at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\libraries\ESP8266mDNS\src/LEAmDNS.cpp:1337
// 0x40219e65 in MDNSPlugin::_loop() at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2\bits/stl_list.h:871
//  (inlined by) MDNSPlugin::_loop() at src\plugins\mdns/mdns_plugin.cpp:347
// 0x4022ef52 in SerialHandler::Wrapper::_loop() at src/serial_handler.cpp:320
// 0x4022ef74 in std::_Function_handler<void (), SerialHandler::Wrapper::begin()::{lambda()#1}>::_M_invoke(std::_Any_data const&) at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2073
// 0x4020e5e9 in loop at c:\users\sascha\.platformio\packages\toolchain-xtensa@2.40802.200502\xtensa-lx106-elf\include\c++\4.8.2/functional:2464
//  (inlined by) loop at src/kfc_firmware.cpp:513
// 0x40100714 in ets_post at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/core_esp8266_main.cpp:177
// 0x4026605c in loop_wrapper() at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/core_esp8266_main.cpp:197
// 0x40100679 in cont_wrapper at C:\Users\sascha\.platformio\packages\framework-arduinoespressif8266\cores\esp8266/cont.S:81


// --------------- CUT HERE FOR EXCEPTION DECODER ---------------

//  ets Jan  8 2013,rst cause:2, boot mode:(3,6)

// load 0x4010f000, len 3584, room 16
// tail 0
// chksum 0xb0
// csum 0xb0
// v2843a5ac
// ~ld
//    __DBG_printf("this=%p", this);
//    __DBG_printf("_entities=%p", _entities);
//    __DBG_printf("entities=%u", _entities.size());
//    __DBG_printf("iterator=%p", _iterator);
//    __DBG_printf("end=%p", _entities.end());

    noInterrupts();
    _iterator = _entities.end();
    interrupts();

    // stop proxies if still running
    _remove->stop(_remove);
    _collect->stop(_collect);
}

bool Queue::isUpdateScheduled()
{
    auto client = MQTTClient::getClient();
    if (!client) {
        return false;
    }
    return client->_autoDiscoveryRebroadcast;
}

void Queue::runPublish(uint32_t delayMillis)
{
    if (delayMillis == kDefaultDelay) {
        delayMillis = _client._config.auto_discovery_delay * 1000U; // 0 = disable
    }
    __LDBG_printf("components=%u delay=%u", _client._components.size(), delayMillis);
    if (!_client._components.empty() && delayMillis) {
        uint32_t initialDelay;

        if (_runFlags & RunFlags::FORCE_NOW) {
            delayMillis = 1;
        }

        // add +-10%
        srand(micros());
        initialDelay = stdex::randint((delayMillis * 90) / 100, (delayMillis * 110) / 100);

        clear();
        _diff = {};
        _entities = List(_client._components, FormatType::JSON);

        __LDBG_printf("starting broadcast in %u ms", std::max<uint32_t>(1000, initialDelay));

        _Timer(_timer).add(Event::milliseconds(std::max<uint32_t>(1000, initialDelay)), false, [this](Event::CallbackTimerPtr timer) {

            // check if we have real time
            if (!(_runFlags & RunFlags::FORCE_NOW)) {
                auto now = time(nullptr);
                if (!IS_TIME_VALID(now)) {
                    __LDBG_printf("time() is invalid, retrying auto discovery in 30 seconds");
                    timer->rearm(Event::seconds(30), false);
                    return;
                }
                if (!_client.isAutoDiscoveryLastTimeValid()) {
                    __LDBG_printf("last update timestamp invalid, retrying auto discovery in 30 seconds");
                    timer->rearm(Event::seconds(30), false);
                    return;
                }
                // check when the next auto discovery is supposed to run
                __LDBG_printf("last_success=%d last_failure=%d run=%d", _client._autoDiscoveryLastSuccess, _client._autoDiscoveryLastFailure, _client._autoDiscoveryLastSuccess > _client._autoDiscoveryLastFailure);
                if (_client._autoDiscoveryLastSuccess > _client._autoDiscoveryLastFailure) {
                    auto cfg = Plugins::MQTTClient::getConfig();
                    uint32_t next = _client._autoDiscoveryLastSuccess + (cfg.getAutoDiscoveryRebroadcastInterval() * 60);
                    int32_t diff = next - time(nullptr);
                    __LDBG_printf("last_published=%d wait_time=%d minutes", (diff / 60) + 1);
                    // not within 2 minutes... report error and delay next run by the time that has been left
                    if (diff > 120) {
                        _publishDone(StatusType::DEFERRED, (next / 60) + 1);
                        return;
                    }
                }
                else {
                    __LDBG_printf("failure timestamp more recent than success, ignoring delay");
                }
            }

            if (_callback) {
                _callback(StatusType::STARTED);
            }

            // get all topics that belong to this device
            __LDBG_printf("starting CollectTopicsComponent");
            _collect.reset(new CollectTopicsComponent(&_client, std::move(_client._createAutoDiscoveryTopics())));

            _collect->begin([this](ComponentProxy::ErrorType error, AutoDiscovery::CrcVector &crcs) {
                __LDBG_printf("collect callback error=%u", error);
                if (error == ComponentProxy::ErrorType::SUCCESS) {
                    // compare with current auto discovery
                    auto currentCrcs = _entities.crc();
                    if (_runFlags & RunFlags::FORCE_UPDATE) {
                        crcs.invalidate();
                    }
                    _diff = currentCrcs.difference(crcs);
                    if (_runFlags & RunFlags::FORCE_UPDATE) {
                        currentCrcs = {};
                        crcs.clear();
                    }
                    //if (currentCrcs == crcs) {
                    if (_diff.equal) {
                        __LDBG_printf("auto discovery matches current version");
                        _publishDone(StatusType::SUCCESS);
                        return;
                    }
                    else {
                        __LDBG_printf("add=%u modified=%u removed=%u unchanged=%u force_update=%u", _diff.add, _diff.modify, _diff.remove, _diff.unchanged, _runFlags & RunFlags::FORCE_UPDATE);
                        // discovery has changed, remove all topics that are not updated/old and start broadcasting
                        // recycle _wildcards and currentCrcs
                        _remove.reset(new RemoveTopicsComponent(&_client, std::move(_collect->_wildcards), std::move(currentCrcs)));
                        // destroy object. the callback was moved onto the stack and _wildcards to RemoveTopicsComponent
                        __LDBG_printf("destroy CollectTopicsComponent");
                        _collect.reset();

                        _remove->begin([this](ComponentProxy::ErrorType error) {
                            __LDBG_printf("collect callback error=%u remove=%p", error, _remove.get());
                            _remove->_wildcards.clear(); // remove wildcards to avoid unsubscribing twice
                            _remove.reset();

                            if (error == ComponentProxy::ErrorType::SUCCESS) {
                                __LDBG_printf("starting to broadcast auto discovery");
                                _packetId = 0;
                                _iterator = _entities.begin();
                                _publishNextMessage();
                            }
                            else {
                                __LDBG_printf("error %u occurred during remove topics", error);
                                _publishDone(StatusType::FAILURE);
                                return;
                            }
                        });
                    }
                }
                else {
                    __LDBG_printf("error %u occurred during collect topics", error);
                    _publishDone(StatusType::FAILURE);
                    return;
                }
            });
        });
    }
    else {
        __LDBG_printf("auto discovery not executed");

        Logger_notice(F("MQTT auto discovery deferred"));
        if (_callback) {
            _callback(StatusType::DEFERRED);
        }

        // cleanup before deleteing
        clear();
        _client._autoDiscoveryQueue.reset(); // deletes itself and the timer
        return;
    }
}

void Queue::_publishNextMessage()
{
    do {
        if (_iterator == _entities.end()) {
            __LDBG_printf("auto discovery published");
            _publishDone(StatusType::SUCCESS);
            return;
        }
        if (!_client.isConnected()) {
            __LDBG_printf("lost connection: auto discovery aborted");
            // _client._startAutoDiscovery = true;
            _publishDone(StatusType::FAILURE);
            return;
        }
        const auto &entitiy = *_iterator;
        if (!entitiy) {
            __LDBG_printf_E("entity nullptr");
            ++_iterator;
            continue;
        }
        auto topic = entitiy->getTopic().c_str();
        auto size = entitiy->getMessageSize();
        if (Client::_isMessageSizeExceeded(size, topic)) {
            // skip message that does not fit into the tcp buffer
            ++_iterator;
            __LDBG_printf_E("max size exceeded topic=%s payload=%u size=%u", topic, entitiy->getPayload().length(), size);
            __LDBG_printf("%s", printable_string(entitiy->getPayload().c_str(), entitiy->getPayload().length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
            continue;
        }
        if (size >= _client.getClientSpace()) {
            __LDBG_printf_W("topic=%s size=%u/%u delay=%u", topic, size, _client.getClientSpace(), kAutoDiscoveryErrorDelay);
            _Timer(_timer).add(Event::milliseconds(kAutoDiscoveryErrorDelay), false, [this](Event::CallbackTimerPtr) {
                _publishNextMessage();
            });
            break;
        }
        __LDBG_printf("topic=%s size=%u/%u", topic, size, _client.getClientSpace());
        // __LDBG_printf("%s", printable_string(entitiy->getPayload().c_str(), entitiy->getPayload().length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
        __LDBG_printf("%s", entitiy->getPayload().c_str());

        // we can assign a packet id before in case the acknowledgment arrives before setting _packetId after the call
        // pretty unlikely with WiFI a a few ms lag, but with enough debug code in between it is possible ;)
        _packetId = _client.getNextInternalPacketId();
        __LDBG_printf("auto discovery packet id=%u", _packetId);
        auto queue = _client.publishWithId(nullptr, entitiy->getTopic(), true, entitiy->getPayload(), QosType::AUTO_DISCOVERY, _packetId);
        if (!queue) {
            __LDBG_printf("failed to publish topic=%s", topic);
            _publishDone(StatusType::FAILURE);
            return;
        }
        // _packetId = queue.getInternalId();
        // __LDBG_printf("auto discovery packet id=%u", _packetId);
    }
    while(false);
}

// make sure to exit any method after calling done
// the method deletes the object and this becomes invalid
void Queue::_publishDone(StatusType result, uint16_t onErrorDelay)
{
    __LDBG_printf("result=%u error_delay=%u entities=%u size=%u crc=%08x add=%u modified=%u removed=%u unchanged=%u",
        result,
        onErrorDelay,
        _entities.size(),
        _entities.payloadSize(),
        _crcs.crc32b(),
        _diff.add, _diff.modify, _diff.remove, _diff.unchanged
    );
    MQTTClient::unregisterComponent(this);

    Event::milliseconds delay;
    if (result == StatusType::SUCCESS) {
        delay = Event::minutes(Plugins::MQTTClient::getConfig().getAutoDiscoveryRebroadcastInterval());
    }
    else if (onErrorDelay) {
        delay = Event::minutes(onErrorDelay);
    }
    else {
        delay = Event::milliseconds::zero();
    }

    auto resultStr = F("unknown");
    switch(result) {
        case StatusType::SUCCESS:
            resultStr = F("published");
            _client.updateAutoDiscoveryTimestamps(true);
            break;
        case StatusType::FAILURE:
            resultStr = F("aborted");
            _client.updateAutoDiscoveryTimestamps(false);
            break;
        case StatusType::DEFERRED:
            resultStr = F("deferred");
            break;
        default:
            __DBG_printf("invalid result %u", result);
            break;
    }
    if (_callback) {
        _callback(result);
    }

    auto message = PrintString(F("MQTT auto discovery %s [entities=%u"), resultStr, _entities.size());
    if (_entities.payloadSize()) {
        message.print(F(", size="));
        message.print(formatBytes(_entities.payloadSize()));
    }
    if (_diff.add) {
        message.printf_P(PSTR(", added %u"), _diff.add);
    }
    if (_diff.remove) {
        message.printf_P(PSTR(", removed %u"), _diff.remove);
    }
    if (_diff.modify) {
        message.printf_P(PSTR(", modified %u"), _diff.modify);
    }
    if (_diff.unchanged) {
        message.printf_P(PSTR(", unchanged %u"), _diff.unchanged);
    }
    if (delay.count()) {
        message.printf_P(PSTR(", rebroadcast in %s"), formatTime(delay.count() / 1000U).c_str());
        _Timer(_client._autoDiscoveryRebroadcast).add(delay, false, Client::publishAutoDiscoveryCallback);
    }
    message.print(']');

    Logger_notice(message);

    // cleanup before deleteing
    clear();
    _client._autoDiscoveryQueue.reset(); // deletes itself and the timer
    return;
}
