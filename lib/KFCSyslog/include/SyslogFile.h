/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class SyslogFile : public Syslog {
public:
    static constexpr size_t kMaxFileSize = 0xffff;
    static constexpr uint16_t kKeepRotatedFilesLimit = 10;

public:
    SyslogFile(SyslogParameter &parameter, const String &filename, size_t maxSize = kMaxFileSize, uint16_t maxRotate = kKeepRotatedFilesLimit);

    void addHeader(String &buffer) override;
    void transmit(const String &message, Callback_t callback) override;
    bool canSend() const override;

    void _rotateLogfile(const String &filename, uint16_t maxRotate);

private:
    String _filename;
    uint32_t _maxSize;
    uint16_t _maxRotate;
};
