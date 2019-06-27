/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>

// use HTTPClient class
#define USE_HTTP_CLIENT				1

#if USE_HTTP_CLIENT && defined(ESP8266)
#include <ESP8266HttpClient.h>
#endif

typedef std::function<void(bool status, const String message, time_t zoneEnd)> RemoteTimezoneStatusCallback_t;

class RemoteTimezone {
public:
	RemoteTimezone();

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
};
