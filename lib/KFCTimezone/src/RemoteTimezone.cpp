/**
* Author: sascha_lammers@gmx.de
*/

#include <JsonCallbackReader.h>
#include <HeapStream.h>
#include <PrintString.h>
#include "Timezone.h"
#include "RemoteTimezone.h"

#if DEBUG_REMOTE_TIMEZONE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if TIMEZONE_USE_HTTP_CLIENT
	#if defined(ESP8266)
		#include <ESP8266HttpClient.h>
	#elif defined(ESP32)
		#include <HTTPClient.h>
	#endif
#else
	#include "asyncHTTPrequest.h"
#endif

RemoteTimezone::RemoteTimezone() : _zoneEnd(0), _callback(nullptr)
{
	_debug_println(_sharedEmptyString);
#if TIMEZONE_USE_HTTP_CLIENT == 0
	_httpClient = nullptr;
#endif
}

RemoteTimezone::~RemoteTimezone()
{
	_debug_println(_sharedEmptyString);
#if TIMEZONE_USE_HTTP_CLIENT == 0
	if (_httpClient) {
		delete _httpClient;
	}
#endif
}

void RemoteTimezone::setUrl(const String url)
{
	_url = url;
}

void RemoteTimezone::setTimezone(const String zoneName)
{
	_zoneName = zoneName;
}

void RemoteTimezone::setStatusCallback(RemoteTimezoneStatusCallback_t callback)
{
	_callback = callback;
}

void RemoteTimezone::get()
{
	_debug_println(_sharedEmptyString);
	_zoneEnd = 0;

	String url = _url;
	if (_zoneName.length()) {
		url.replace(F("${timezone}"), _zoneName);
	}

#if TIMEZONE_USE_HTTP_CLIENT
	HTTPClient http;

	_debug_printf_P(PSTR("url=%s\n"), url.c_str());

	http.begin(url);
	int httpCode = http.GET();
	if (httpCode == 200) {
		Stream &stream = http.getStream();
		_responseHandler(url, stream);
	} else {
		if (_callback) {
			PrintString message;
			message.printf_P(PSTR("HTTP error, code %d"), httpCode);
			_callback(false, message, 0);
		}
	}
	http.end();
#else

	_debug_printf_P(PSTR("url=%s\n"), url.c_str());

	if (_httpClient != nullptr) {
		delete _httpClient;
	}

	_httpClient = new asyncHTTPrequest();
	_httpClient->onReadyStateChange([url, this](void *, asyncHTTPrequest *request, int readyState) {
		if (readyState == 4) {
			int httpCode;
			if ((httpCode = request->responseHTTPcode()) == 200) {
				String response = request->responseText();
				HeapStream stream(response);
				_responseHandler(url, stream);
			}
			else {
				if (_callback) {
					PrintString message;
					message.printf_P(PSTR("HTTP error, code %d"), httpCode);
					_callback(false, message, 0);
				}
			}
		}
	});
	_httpClient->setTimeout(30);

	if (!_httpClient->open("GET", url.c_str()) || !_httpClient->send()) {
		if (_callback) {
			_callback(false, F("HTTP Client failed"), 0);
		}
	}
#endif
}

time_t RemoteTimezone::getZoneEnd() {
	return _zoneEnd;
}

void RemoteTimezone::_responseHandler(const String url, Stream &stream)
{
	_debug_printf_P(PSTR("url=%s\n"), url.c_str());

	struct {
		bool status;
		String message;
		String zoneName;
		String abbreviation;
		uint32_t offset;
		time_t zoneEnd;
		bool dst;
	} args;

	args.status = false;
	args.offset = 0;
	args.zoneEnd = 0;

	JsonCallbackReader json = JsonCallbackReader(stream, [&args](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) {
		if (json.getLevel() == 1) {
			if (key.equals(F("status")) && value.equalsIgnoreCase(F("OK"))) {
				args.status = true;
			} else if (key.equals(F("message"))) {
				args.message = value;
			} else if (key.equals(F("zoneName"))) {
				args.zoneName = value;
			} else if (key.equals(F("abbreviation"))) {
				args.abbreviation = value;
			} else if (key.equals(F("gmOffset"))) {
				args.offset = value.toInt();
			} else if (key.equals(F("zoneEnd"))) {
				args.zoneEnd = value.toInt();
			} else if (key.equalsIgnoreCase(F("DST"))) {
				args.dst = value.toInt() ? true : false;
			}
		}
		return true;
	});

	if (json.parse()) {
		if (args.status) {
			Timezone &tz = get_default_timezone();

#if DEBUG
			PrintString message;
			message.printf_P(PSTR("RemoteTimezone response zoneName %s abbreviation %s DST %d offset %ld zone end %lu\n"), args.zoneName.c_str(), args.abbreviation.c_str(), args.dst, (long)args.offset, (unsigned long)args.zoneEnd);
			args.message = message;
#endif

			tz.setAbbreviation(args.abbreviation);
			tz.setTimezone(0, args.zoneName);
			tz.setDst(args.dst);
			tz.setOffset(args.offset);
			tz.save();
		}
	} else {
		args.status = false;
		args.message = json.getLastErrorMessage();
	}
	_zoneEnd = args.zoneEnd;
    if (_zoneEnd <= 0) {
        _zoneEnd = time(nullptr) + 86400; // retry in 24 hours if not available
    }

	if (_callback) {
		_callback(args.status, args.message, args.zoneEnd);
	}
}
