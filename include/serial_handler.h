/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_SERIAL_HANDLER
#define DEBUG_SERIAL_HANDLER        0
#endif

#include <Arduino.h>
#include <functional>
#include <vector>
#include <Stream.h>
#include <StreamWrapper.h>
#include <NullStream.h>
#include <Buffer.h>
#include <LoopFunctions.h>
#include <EventScheduler.h>
#include <HardwareSerial.h>
#include <EnumHelper.h>
#include <cbuf.h>


#ifndef SERIAL_HANDLER_INPUT_BUFFER_MAX
#define SERIAL_HANDLER_INPUT_BUFFER_MAX                     512
#endif

#if  0
#define DEBUG_SH2(fmt,...)          { ::printf(PSTR("SH2:" fmt "\n"), ##__VA_ARGS__); }
#else
#define DEBUG_SH2(...)              ;
#endif

namespace SerialHandler {

    enum class EventType : uint8_t {
        NONE =          0,
        WRITE =         0x01,
        READ =          0x02,
    };

    class Client;
    class Wrapper;

    using Callback = std::function<void(Client &client)>;

    class Client : public Stream {
    public:
        Client(Callback cb, EventType events);

        bool operator==(const Client &cb) {
            return &cb == this;
        }

    private:
        friend Wrapper;

        cbuf &_getRx() {
            return _rx;
        }
        cbuf &_getTx() {
            return _tx;
        }
        EventType _getEvents() const {
            return _events;
        }
        bool _has(EventType event) const {
            return EnumHelper::Bitset::hasAny(_events, event);
        }

    // Stream
    public:
        virtual int available();
        virtual int read();
        virtual int peek();

    // Print
    public:
        virtual size_t write(uint8_t);
        virtual size_t write(const uint8_t *buffer, size_t size);

    private:
        Callback _cb;
        EventType _events;
        cbuf _rx;
        cbuf _tx;
    };


    class Wrapper : public StreamWrapper
    {
    public:
        using ClientsVector = std::vector<Client>;
        static constexpr size_t kMaxBufferSize = 1024;
        static constexpr size_t kMinBufferSize = 128;
        static constexpr size_t kAddBufferSize = 128; // if not enough room, increase buffer size if less than max. size

        Client &addClient(Callback cb, EventType events);
        void removeClient(const Client &client);

    public:
        Wrapper(Stream *stream);
        virtual ~Wrapper();

        void begin();
        void end();

    public:
        virtual int available() override {
            return 0;
        }
        virtual int read() override {
            return -1;
        }
        virtual int peek() override {
            return -1;
        }
        virtual size_t readBytes(char *buffer, size_t length) override {
            return 0;
        }

    public:
        // send data to serial port and all clients
        virtual size_t write(uint8_t data) override;
        virtual size_t write(const uint8_t *buffer, size_t size) override;

    private:
        // write data that is received by serial or written to serial
        void _writeClientsRx(Client *src, const uint8_t *buffer, size_t size, EventType type);
        // write data to serial and other clients
        void _writeClientsTx(Client *src, const uint8_t *buffer, size_t size);

        // read serial data
        void _pollSerial();
        // invoke clients callbacks to read the rx buffer
        void _transmitClientsRx();
        // transmit clients tx buffer to serial and other clients
        void _transmitClientsTx();

        void _loop();

    private:
        friend Client;

        ClientsVector _clients;
        bool _txFlag; // indicator that _clients have _tx with data
    };

};

extern NullStream NullSerial;
extern HardwareSerial Serial0;
extern Stream &Serial;
extern Stream &DebugSerial;
extern SerialHandler::Wrapper serialHandler;
