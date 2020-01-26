/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <KFCRestApi.h> // TODO change request to restapi class

#ifndef DEBUG_REMOTE_TIMEZONE
#define DEBUG_REMOTE_TIMEZONE 					0
#endif

#if _WIN32 || _WIN64
#define TIMEZONE_USE_HTTP_CLIENT				KFC
#elif defined(ESP32)
#define TIMEZONE_USE_HTTP_CLIENT				1
#else0
#define TIMEZONE_USE_HTTP_CLIENT				0
#endif

typedef std::function<void(bool status, const String &message, time_t zoneEnd)> RemoteTimezoneStatusCallback_t;

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
