/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class SyslogFile : public Syslog {
public:
    static constexpr size_t kMaxFileSize = 0xffff;
    static constexpr uint16_t kKeepRotatedFilesLimit = 10;
    static constexpr uint32_t kMaxFilesizeMask = 0xffffff;

    // pack max. filesize and rotation limit into uint32_t for SyslogFactory
    static uint32_t getPackedFilesize(size_t maxSize = kMaxFileSize, uint8_t maxRotate = kKeepRotatedFilesLimit) {
        return ((maxSize >> 2) & kMaxFilesizeMask) | (maxRotate << 24);
    }

public:
    SyslogFile(const char *hostname, SyslogQueue *queue, const String &filename, size_t maxSize = kMaxFileSize, uint16_t maxRotate = kKeepRotatedFilesLimit);

    virtual bool setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port);
    virtual void transmit(const SyslogQueueItem &item);
    virtual uint32_t getState(StateType state);
    virtual String getHostname() const;
    virtual uint16_t getPort() const;

    void _rotateLogfile(const String &filename, uint16_t maxRotate);

private:
    String _getHeader();

    String _filename;
    uint32_t _maxSize;
    uint16_t _maxRotate;
};

inline bool SyslogFile::setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port)
{
    return false;
}

inline uint16_t SyslogFile::getPort() const
{
    return 0;
}

inline String SyslogFile::getHostname() const
{
    return String(F("file://")) + _filename;
}
