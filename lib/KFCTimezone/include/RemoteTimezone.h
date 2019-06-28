/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>

#if _WIN32 || _WIN64
#define TIMEZONE_USE_HTTP_CLIENT				1
#else
// use HTTPClient class
#define TIMEZONE_USE_HTTP_CLIENT				0
#endif

typedef std::function<void(bool status, const String message, time_t zoneEnd)> RemoteTimezoneStatusCallback_t;

class RemoteTimezone {
public:
	RemoteTimezone();
	virtual ~RemoteTimezone();

	void setUrl(const String url);
	void setTimezone(const String zoneName);
	void setStatusCallback(RemoteTimezoneStatusCallback_t callback);

	void get();
	time_t getZoneEnd();

private:
	void _responseHandler(const String url, Stream &stream);

	String _url;
	String _zoneName;
	time_t _zoneEnd;
	RemoteTimezoneStatusCallback_t _callback;

#if TIMEZONE_USE_HTTP_CLIENT == 0
	class asyncHTTPrequest *_httpClient;
#endif
};
