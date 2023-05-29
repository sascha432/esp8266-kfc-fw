/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <WiFiUdp.h>
#include <EventScheduler.h>
#include "serial_handler.h"

class Serial2Udp {
public:

    static void initWiFi(const String &SSID, const String &password, const IPAddress &address, uint16_t port, uint flushDelay = 25, uint32_t WiFiTimeout = 5000) {
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        WiFi.enableSTA(true);
        WiFi.begin(SSID.c_str(), password.c_str());
        WiFi.waitForConnectResult(
            #if ESP8266
                WiFiTimeout
            #endif
        );
        static Serial2Udp serial2UDP(address, port);
        Serial.println(F("BOOTING..."));
    }

    Serial2Udp(const IPAddress &address, uint16_t port, uint flushDelay = 25) : _address(address), _port(port), _client(serialHandler.addClient([this, flushDelay](Stream &stream) {
            if (stream.available()) {
                if (!_flushTimer) {
                    _Timer(_flushTimer).add(Event::milliseconds(flushDelay), false, [this, &stream](Event::CallbackTimerPtr timer) {
                        WiFiUDP udp;
                        if (udp.beginPacket(_address, _port)) {
                            while(stream.available()) {
                                if (udp.write(static_cast<uint8_t>(stream.peek())) != 1) {
                                    break;
                                }
                                stream.read();
                            }
                            udp.endPacket();
                        }
                    }, Event::PriorityType::TIMER);
                }
            }
        }, SerialHandler::EventType::RW)) {}
        ~Serial2Udp() {
            serialHandler.removeClient(_client);
        }
private:
    IPAddress _address;
    uint16_t _port;
    Event::Timer _flushTimer;
    SerialHandler::Client &_client;
};
