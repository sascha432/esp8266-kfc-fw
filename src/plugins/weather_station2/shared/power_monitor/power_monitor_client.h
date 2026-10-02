/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// TCP client of the rpi-power-monitor server (https://github.com/sascha432/rpi-power-monitor).
//
// The server streams a raw binary TCP protocol: fixed 40-byte little-endian frames, back to
// back, without framing and without a length prefix (see docs/protocol.md of the server). One
// sample is one frame per channel, all frames of a sample share the same timestamp. Besides the
// periodic samples the server sends a one-shot "daily energy" block right after a client
// connects; those frames carry a reserved channel id and are skipped here.
//
// Reading the stream blocks, so the client runs a reader task (the same pattern the
// OpenWeatherMap client of the weather station uses). The task connects, decodes the frames and
// stores the latest sample per channel under a mutex, the main loop reads the samples. A lost
// connection is retried every kReconnectDelay, the target is read again on every reconnect so a
// configuration change applies without restarting the task.

#include <Arduino_compat.h>
#include <Mutex.h>

namespace WeatherStation2 {

namespace PowerMonitor {

// One frame of the stream. The layout is byte identical to the natural x86-64 layout of the
// server's struct, the 4 bytes of padding are explicit so the wire size stays 40 bytes on every
// target:
//
//   offset size type    field
//   ------ ---- ------- --------------------------------------------------------------
//   0      4    uint32  channel_id                 1..3 rails, 100+idx aggregates
//   4      4    uint32  timestamp_millis           ms since the server started (monotonic)
//   8      4    uint32  voltage_millivolt          rails only, 0 on aggregates
//   12     4    int32   current_milliamps          rails only, 0 on aggregates
//   16     4    int32   power_milliwatt            rails: own, aggregates: summed
//   20     4    -       padding
//   24     8    int64   energy_milliwatthours        since this server run (ignored)
//   32     8    int64   energy_milliwatthours_total  persistent total (used)
#pragma pack(push, 1)
struct Frame {
    uint32_t channelId;
    uint32_t timestamp;
    uint32_t voltageMillivolt;
    int32_t currentMilliamps;
    int32_t powerMilliwatt;
    uint32_t padding;
    int64_t energyMilliwatthours;
    int64_t energyMilliwatthoursTotal;
};
#pragma pack(pop)

static_assert(sizeof(Frame) == 40, "the power monitor frame has to stay 40 bytes");

// channel ids with bit 31 set are reserved control frames (the one-shot daily energy block). They
// are neither a rail nor an aggregate and are ignored by the client
static constexpr uint32_t kControlChannelId = 0x80000000;

// latest sample of one channel
struct Sample {
    bool valid{false};
    float voltage{0};       // V
    float current{0};       // A
    float power{0};         // W
    double energy{0};       // kWh, the total (persistent) counter of the server
    uint32_t timestamp{0};  // timestamp of the server, ms since it started
};

class Client {
public:
    static constexpr uint16_t kDefaultPort = 7000;
    // connect timeout, a host that does not answer must not block the reader forever
    static constexpr uint32_t kConnectTimeout = 5000;
    // wait before a lost or failed connection is retried
    static constexpr uint32_t kReconnectDelay = 5000;
    // stack of the reader task (raw TCP plus name resolution)
    static constexpr uint32_t kTaskStack = 6144;
    // number of distinct channel ids that are kept. The shipped configuration of the server sends
    // 5 (3 rails plus 2 aggregates), the limit only protects against a server that sends more
    static constexpr uint8_t kMaxSamples = 12;

    Client();
    ~Client();

    // creates the reader task, does nothing while it is already running
    void begin();
    // Target of the next connection, the reader picks it up on the next reconnect. An empty host
    // keeps the task idle, the channels report "not configured" in that case
    void setTarget(const String &host, uint16_t port);
    // asks the reader task to end and waits for it
    void stop();

    // true while a target is configured
    bool isConfigured() const;
    bool isRunning() const {
        return _task != nullptr;
    }
    // true while the TCP connection to the server is up
    bool isConnected() const;
    // copies the latest sample of a channel, false while none was received yet
    bool getSample(uint32_t channelId, Sample &sample) const;
    // number of samples since the connection was established
    uint32_t getSampleCount() const;
    // milliseconds since the last sample arrived, 0 while none was received
    uint32_t getLastSampleAge() const;
    // error of the last connection attempt, empty while it succeeded
    String getError() const;

private:
    static void _taskEntry(void *arg);
    // connects, reads frames and reconnects until _stop is set
    void _readLoop();
    // reads the target under the lock, false while no host is configured
    bool _getTarget(String &host, uint16_t &port) const;
    // decodes one frame into the sample table
    void _store(const Frame &frame);
    // connection state and error, written by the reader task
    void _setConnection(bool connected, const char *error);

private:
    mutable SemaphoreMutex _lock;
    String _host;
    uint16_t _port{kDefaultPort};
    volatile bool _stop{false};
    void *_task{nullptr};
    // written by the reader task, read by the main loop, both under _lock
    bool _connected{false};
    uint32_t _sampleCount{0};
    uint32_t _lastSampleMillis{0};
    String _error;
    struct Entry {
        uint32_t channelId{0};
        Sample sample;
    };
    Entry _entries[kMaxSamples];
    uint8_t _entryCount{0};
};

} // namespace PowerMonitor

} // namespace WeatherStation2
