/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_SERIAL_HANDLER
#define DEBUG_SERIAL_HANDLER                                0
#endif

#include <Arduino.h>
#include <functional>
#include <list>
#include <Stream.h>
#include <StreamWrapper.h>
#include <NullStream.h>
#include <Buffer.h>
#include <EventScheduler.h>
#include <HardwareSerial.h>
#include <EnumHelper.h>
#include <cbuf.h>

#ifndef SERIAL_HANDLER_INPUT_BUFFER_MAX
#define SERIAL_HANDLER_INPUT_BUFFER_MAX 512
#endif

namespace SerialHandler {

    enum class EventType : uint8_t {
        NONE =          0,
        WRITE =         0x01,
        READ =          0x02,
        RW =            READ|WRITE,
    };

    class Client;
    class Wrapper;

    using Callback = std::function<void(Stream &client)>;

    class Client : public Stream {
    public:
        Client(const Client &) = delete;
        Client &operator=(const Client &) = delete;

        Client();
        Client(const Callback &cb, EventType events);

        bool operator==(const Client &cb);
        EventType getEvents() const;

        // set events and allocate buffers
        void start(EventType events);
        // remove ends and free buffers
        void stop();

    private:
        friend Wrapper;

        cbuf &_getRx();
        cbuf &_getTx();
        EventType _getEvents() const;
        bool _hasAny(EventType event) const;

        void _allocateBuffers();
        void _freeBuffers();
        void _checkBufferSize(cbuf &buf, size_t size);
        void _resizeBufferMinSize(cbuf &buf);

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
        using ClientPtr = std::unique_ptr<Client>;
        using Clients = std::list<ClientPtr>;

        // initial size
        static constexpr size_t kMinBufferSize = 128;
        // max. size before resizing stops
        static constexpr size_t kMaxBufferSize = 2048;
        // if not enough room, increase buffer size if less than kMaxBufferSize
        static constexpr size_t kAddBufferSize = 256;
        // max. size it can reach
        static constexpr size_t kRealMaxBufferSize = ((((kMaxBufferSize - kMinBufferSize) + kAddBufferSize - 1) / kAddBufferSize) * kAddBufferSize) + kMinBufferSize;
        // set buffer to this initial value
        static constexpr size_t kInitBufferSize = 8;

        // the returned reference is only valid until Wrapper.end() is called or the client is removed
        Client &addClient(const Callback &cb, EventType events);
        void removeClient(const Client &client);

    public:
        Wrapper(Stream *stream);
        virtual ~Wrapper();

        void begin();
        void end();

        void addLoop();
        void removeLoop();
        static void pollSerial();

    public:
        virtual int available() override;
        virtual int read() override;
        virtual int peek() override;
        virtual size_t readBytes(char *buffer, size_t length) override;

        Clients &getClients();
        static Wrapper &getInstance();

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

        Clients _clients;
        bool _txFlag; // indicator that _clients have _tx with data
    };

    //
    // Client
    //

    inline bool Client::operator==(const Client &cb)
    {
        return std::addressof(cb) == this;
    }

    inline EventType Client::getEvents() const
    {
        return _getEvents();
    }

    inline cbuf &Client::_getRx()
    {
        return _rx;
    }

    inline cbuf &Client::_getTx()
    {
        return _tx;
    }

    inline EventType Client::_getEvents() const
    {
        return _events;
    }

    inline bool Client::_hasAny(EventType event) const
    {
        return EnumHelper::Bitset::hasAny(_events, event);
    }

    inline int Client::available()
    {
        return _rx.available();
    }

    inline int Client::read()
    {
        return _rx.read();
    }

    inline int Client::peek()
    {
        return _rx.peek();
    }

    //
    // Wrapper
    //

    inline Wrapper::~Wrapper()
    {
        end();
    }

    inline void Wrapper::begin()
    {
        end();
        addLoop();
    }

    inline void Wrapper::end()
    {
        removeLoop();
        _clients.clear();
    }

    inline void Wrapper::addLoop()
    {
        // LoopFunctions::add(pollSerial);
        LoopFunctions::add([this]() {
            this->_loop();
        }, this);
    }

    inline void Wrapper::removeLoop()
    {
        // LoopFunctions::remove(pollSerial);
        LoopFunctions::remove(this);
    }

    inline Client &Wrapper::addClient(const Callback &cb, EventType events)
    {
        _clients.emplace_back(new Client(cb, events));
        return *_clients.back().get();
    }

    inline int Wrapper::available()
    {
        return 0;
    }

    inline int Wrapper::read()
    {
        return -1;
    }

    inline int Wrapper::peek()
    {
        return -1;
    }

    inline size_t Wrapper::readBytes(char *buffer, size_t length)
    {
        return 0;
    }

    inline size_t Wrapper::write(uint8_t data)
    {
        return write(&data, 1);
    }

    inline Wrapper::Clients &Wrapper::getClients()
    {
        return _clients;
    }

    inline void Wrapper::_loop()
    {
        _pollSerial();
        _transmitClientsRx();
        _transmitClientsTx();
    }

};

extern NullStream NullSerial;
extern HardwareSerial Serial0;
extern Stream &Serial;
extern Stream &DebugSerial;
extern SerialHandler::Wrapper serialHandler;
extern StreamWrapper debugStreamWrapper;

namespace SerialHandler {

    inline Wrapper &Wrapper::getInstance()
    {
        return serialHandler;
    }

    inline void Wrapper::pollSerial()
    {
        serialHandler._pollSerial();
    }

}
