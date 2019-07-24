/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define SYSLOG_FILE_MAX_SIZE 0xffff
#define SYSLOG_FILE_MAX_ROTATE 10

class SyslogFile : public Syslog {
   public:
    SyslogFile(SyslogParameter &parameter, const String filename, size_t maxSize = SYSLOG_FILE_MAX_SIZE, uint16_t maxRotate = SYSLOG_FILE_MAX_ROTATE);

    void addHeader(String &buffer) override;
    void transmit(const char *message, size_t length, SyslogCallback callback) override;
    bool canSend() const override;

    void _rotateLogfile(const String filename, uint16_t maxRotate);

   private:
    String _filename;
    uint32_t _maxSize;
    uint16_t _maxRotate;
};
