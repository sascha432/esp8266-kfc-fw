/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class SyslogFile : public Syslog {
public:
    static constexpr size_t kMaxFileSize = 0xffff;
    static constexpr uint16_t kKeepRotatedFilesLimit = 10;

public:
    SyslogFile(SyslogParameter &&parameter, SyslogQueue &queue, const String &filename, size_t maxSize = kMaxFileSize, uint16_t maxRotate = kKeepRotatedFilesLimit);

    virtual bool setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port);
    virtual void transmit(const SyslogQueueItem &item);
    virtual bool canSend() const;
    virtual bool isSending();

    virtual String getHostname() const;
    virtual uint16_t getPort() const;

    void _rotateLogfile(const String &filename, uint16_t maxRotate);

private:
    String _getHeader();

    String _filename;
    uint32_t _maxSize;
    uint16_t _maxRotate;
};
