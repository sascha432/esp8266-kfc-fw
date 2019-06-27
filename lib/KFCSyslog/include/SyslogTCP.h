/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define SYSLOG_PORT_TCP 514
#define SYSLOG_PORT_TCP_TLS 6514

class SyslogTCP : public Syslog {
public:
    SyslogTCP(SyslogParameter &parameter, const char *host, uint16_t port = SYSLOG_PORT_TCP, bool useTLS = false);

	void transmit(const char *message, size_t length, SyslogCallback callback) override;
	bool isSending() override;

private:
    String _host;
    uint16_t _port;
    bool _useTLS;
};
