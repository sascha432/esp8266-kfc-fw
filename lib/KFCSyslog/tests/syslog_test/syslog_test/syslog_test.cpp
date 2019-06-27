/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogFactory.h"
#include "SyslogFilter.h"
#include "SyslogQueue.h"
#include "SyslogStream.h"

int main() {
	SyslogParameter parameter;
    parameter.setHostname(F("TESTHOST"));
    parameter.setAppName(F("TestApp"));
    parameter.setProcessId(F("Dummy"));
	parameter.setSeverity(SYSLOG_ERR);

    SyslogFilter *filter = new SyslogFilter(parameter);
    filter->addFilter("*.*", "./log/all.log:1024:10");
    filter->addFilter("*.debug", "./log/debug.log:65535:10");
//    filter->addFilter("*.debug", "@192.168.0.61");
	filter->addFilter("*.debug", SYSLOG_FILTER_STOP);
    filter->addFilter("kern.*,user.*", "@192.168.0.61");

	SyslogQueue *queue = new SyslogMemoryQueue(1024);
    //SyslogQueue *queue = new SyslogQueue();
    SyslogStream syslog(filter, queue);

	syslog.setFacility(SYSLOG_FACILITY_USER);
    Syslog_log_P(syslog, SYSLOG_INFO, PSTR("test %s"), syslog.getLevel().c_str());
	B
    WiFi.setIsConnected(false);
    Syslog_log_P(syslog, SYSLOG_ERR, PSTR("test %s"), syslog.getLevel().c_str());
    Syslog_log_P(syslog, SYSLOG_WARN, PSTR("test %s"), syslog.getLevel().c_str());
    Syslog_log_P(syslog, SYSLOG_DEBUG, PSTR("test %s"), syslog.getLevel().c_str());

	syslog.setSeverity(SYSLOG_ERR);

	syslog.setFacility(SYSLOG_FACILITY_KERN);
	syslog.printf_P("kern.err: test %d", 555);
	syslog.flush();

	syslog.setFacility(SYSLOG_FACILITY_USER);
	syslog.printf_P("user.err: test %d", 345);
	syslog.flush();

	syslog.setSeverity(SYSLOG_WARN);
	syslog.printf_P("user.warn: test %d", 123);
	syslog.flush();

    syslog.printf_P("test %d", 123);
    syslog.flush();

    WiFi.setIsConnected(true);

	delay(10000);

	int timeout = 10;
	while (syslog.hasQueuedMessages() && timeout) {
        delay(1000);
        printf("timeout %d...\n", timeout--);
		syslog.deliverQueue();
    }

    return 0;
}
