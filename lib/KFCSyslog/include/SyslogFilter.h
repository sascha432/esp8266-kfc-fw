/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

typedef std::function<void(SyslogFileFilterItem &)> SyslogFilterCallback;

typedef std::vector<SyslogFileFilterItem> SyslogFiltersVector;

typedef std::vector<std::pair<const String, Syslog *>> SyslogObjectsVector;

#define SYSLOG_FILTER_STOP nullptr

class SyslogFilter {
public:
	SyslogFilter(const SyslogParameter &parameter);
	SyslogFilter(const String &hostname, const String &appName);

	/**
	* @HOST[:PORT]             forward to UDP (port is 514 by default)
	* @@HOST[:PORT]            forward to TCP (port 514)
	* @@!HOST[:PORT]           forward to TCP using TLS (port 6514)
	* /var/log/message[:max size[:max rotate]]
	*                          store in file (max size = 64K, max rotate = 10)
	* stop                     discard message
	*/
	void addFilter(const String &filter, const String &destination);
	void addFilter(const String &filter, Syslog *syslog);
	SyslogParameter &getParameter();

	void applyFilters(SyslogFilterCallback callback);

	Syslog *createSyslogFromString(const String &str);

private:
	SyslogFiltersVector _filters;
	SyslogParameter _parameter;
	SyslogObjectsVector _syslogObjects;
};
