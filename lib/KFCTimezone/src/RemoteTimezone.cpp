/**
* Author: sascha_lammers@gmx.de
*/

#include <JsonCallbackReader.h>
#include "Timezone.h"
#include "RemoteTimezone.h"
// #include "logger.h"

#if TIMEZONE_USE_HTTP_CLIENT == 0
#include <BufferStream.h>
#include <PrintString.h>
#include "asyncHTTPrequest.h"
#endif

#if 0

void timezone_call_remote_api();

typedef struct {
	bool status;
	String message;
	String zoneName;
	String abbreviation;
	time_t zoneEnd;
	bool dst;
} CallbackArgs_t;

uint16_t timezone_update_failures;
EventScheduler::EventTimerPtr updateCheckTimer = nullptr;

void timezone_remove_timer() {
	if (updateCheckTimer) {
		if (Scheduler.hasTimer(updateCheckTimer)) {
			Scheduler.removeTimer(updateCheckTimer);
		}
		updateCheckTimer = nullptr;
	}
}

static int32_t next_check_delay;

#if USE_ASYNC_HTTP_REQUEST
void timezone_http_request_ended(asyncHTTPrequest *request) {
#else
void timezone_http_request_ended() {
#endif

#if USE_ASYNC_HTTP_REQUEST
	if_debug_printf_P(PSTR("asyncHTTPrequest ended %p\n"), request);

	Scheduler.addTimer(300 * 1000, false, [request](EventScheduler::EventTimerPtr timer) {  // TODO
		delete request;
	});
#endif

	// schedule next check
	if (next_check_delay) {
		timezone_remove_timer();
		int32_t time_val = next_check_delay;
		updateCheckTimer = Scheduler.addTimer((int64_t)next_check_delay * (int64_t)1000, false, [time_val](EventScheduler::EventTimerPtr timer) {
			if_debug_forms_printf_P(PSTR("retry timer called %d\n"), time_val) updateCheckTimer = nullptr;
			timezone_call_remote_api();
		});
	} else {
		if_debug_printf_P(PSTR("ERROR: next_check_delay = 0, timer was not started"));
	}
}

void timezone_update_callback(bool success, int offset = -1, time_t next_timezone_update = 0, String error = String()) {
	if_debug_printf_P(PSTR("timezone_update_callback(%d, %d, %lu, %s)\n"), success, offset, next_timezone_update, error.c_str());

	next_check_delay = 0;
	if (success) {
		time_t now = time(nullptr);
		sint8_t tzOfs = (offset == -1) ? 0 : offset / 3600;
		sntp_set_timezone(tzOfs);
		// sntp_set_daylight(0);

		if (next_timezone_update) {
			next_check_delay = next_timezone_update - now;
		}
		if (next_check_delay < 86400) {
			next_check_delay = 86400;  // check once daily if time is invalid
		}
		next_timezone_update = now + next_check_delay;

		char buf[32];
		timezone_strftime_P(buf, sizeof(buf), PSTR("%a, %d %b %Y %T %Z"), timezone_localtime(&next_timezone_update));
		Logger_notice(F("Remote timezone: %s offset %0.2d:%0.2u. Next check at %s"), local_timezone.getAbbreviation().c_str(), offset / 3600, offset % 60, buf);

	} else {
		/**
		echo '<?php for($i = 0; $i < 30; $i++) { echo floor(10 * (1 + $i * $i / 3)).", "; } echo "\n";' |php
		*/
		next_check_delay = 10 * (1 + timezone_update_failures * timezone_update_failures / 3);  // 10, 13, 23, 40, 63, 93, 130, 173, 223, 280, 343, 413, 490, 573, 663, 760, 863, 973, 1090, 1213, 1343, 1480, 1623, 1773, 1930, 2093, 2263, 2440, 2623, 2813, ...
		timezone_update_failures++;
		Logger_notice(F("Remote timezone: Update failed. Retrying (#%d) in %d second(s). %s"), timezone_update_failures, next_check_delay, error.c_str());
	}
}

void timezone_response_handler(const String &url, Stream &stream) {
	CallbackArgs_t args;
	args.status = false;
	args.zoneEnd = 0;

	JsonCallbackReader json = JsonCallbackReader(stream, [&args](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) -> bool {
		// if_debug_printf_P(PSTR("JSON variable %d %s=%s\n"), level, key.c_str(), data.c_str());

		if (json.getLevel() == 1) {
			if (key == F("status") && value == FSPGM(OK)) {
				args.status = true;
			} else if (key == F("message")) {
				args.message = value;
			} else if (key == F("zoneName")) {
				args.zoneName = value;
			} else if (key == F("abbreviation")) {
				args.abbreviation = value;
			} else if (key == F("gmOffset")) {
				local_timezone.setOffset(value.toInt());
			} else if (key == F("zoneEnd")) {
				args.zoneEnd = value.toInt();
				if (args.zoneEnd < 1) {  // data not available, set to one day in the future
					args.zoneEnd = time(NULL) + 86400;
				}
			} else if (key == F("dst")) {
				args.dst = value.toInt() ? true : false;
			}
		}
		return true;
	});

	if (json.parse()) {
		if (args.status) {
			local_timezone.setTimezone(0, args.zoneName);
			local_timezone.setAbbreviation(args.abbreviation);
			local_timezone.setDst(args.dst);
			if_debug_printf_P(PSTR("Remote timezone: %s (%s) offset %d dst %d next update %lu\n"), local_timezone.getTimezone().c_str(), local_timezone.getAbbreviation().c_str(), local_timezone.getOffset(), local_timezone.isDst() ? 1 : 0, args.zoneEnd);
			timezone_update_callback(true, local_timezone.getOffset(), args.zoneEnd + 1);
		} else {
			PrintString out;
			out.printf_P(PSTR("Remote timezone url: %s Status %d Message %s"), url.c_str(), args.status, args.message.c_str());
			timezone_update_callback(false, -1, 0, out);
		}
	} else {
		PrintString out;
		out.printf_P(PSTR("Remote timezone url: %s Status %d: %s"), url.c_str(), args.status, json.getLastErrorMessage().c_str());
		timezone_update_callback(false, -1, 0, out);
	}
#if USE_ASYNC_HTTP_REQUEST
	timezone_http_request_ended(request);
#else
	timezone_http_request_ended();
#endif
}

void timezone_call_remote_api() {
	if (!WiFi.isConnected()) {
		if_debug_printf_P(PSTR("WiFi not connected, remote api call deferred\n"));
		return;
	}

	auto &config = _Config.get();
	String url = config.ntp.remote_tz_dst_ofs_url;
	url.replace(F("%s"), config.ntp.timezone);  // TODO remove support for %s
	url.replace(F("${timezone}"), config.ntp.timezone);

#if USE_ASYNC_HTTP_REQUEST

	asyncHTTPrequest *timezone_http_request = new asyncHTTPrequest();
	timezone_http_request->onReadyStateChange([url](void *, asyncHTTPrequest *request, int readyState) {
		// if_debug_printf_P(PSTR("onReadyStateChange readyState %d time %d length %d available e%d http %d\n"), readyState, request->elapsedTime(), request->responseLength(), request->available(), request->responseHTTPcode());

		if (readyState == 4) {
			if (request->responseHTTPcode() == 200) {
				String response = request->responseText();
				if_debug_printf_P(PSTR("timezone http client: HTTP code %d, length %d, response %s\n"), request->responseHTTPcode(), response.length(), response.c_str());
				StreamString reader = StreamString(response);

				timezone_response_handler(url, reader);

			} else {
				PrintString out;
				out.printf_P(PSTR("readyState %d, http code %d, available %d, length %d"), readyState, request->available(), request->responseLength());
				timezone_update_callback(false, -1, 0, out);
				timezone_http_request_ended(url, request);
			}
		}
	});
	timezone_http_request->setTimeout(30);

	if_debug_printf_P(PSTR("timezone remote api %s\n"), url.c_str());

	if (!timezone_http_request->open("GET", url.c_str())) {
		Logger_notice(F("HTTP client: Open failed: %s"), url.c_str());
		timezone_update_callback(false);
		timezone_http_request_ended(timezone_http_request);
	} else if (!timezone_http_request->send()) {
		Logger_notice(F("HTTP client: Send failed: %s"), url.c_str());
		timezone_update_callback(false);
		timezone_http_request_ended(timezone_http_request);
	}

#else

	HTTPClient http;

	if_debug_printf_P(PSTR("timezone remote api %s\n"), url.c_str());

	http.begin(url);
	int httpCode = http.GET();

	debug_printf_P(PSTR("http code %d, response size %d\n"), httpCode, http.getSize());
	timezone_response_handler(url, http.getStream());

	http.end();

#endif
}

void timezone_setup() {
	WebTemplate::registerVariable(F("NTP_STATUS"), timezone_getStatus);
	timezone_update_check_enable();
}

void timezone_wifi_connected_callback(uint8_t event, void *) {
	if_debug_printf_P(PSTR("wifi callback event, timer %d %d\n"), updateCheckTimer, Scheduler.hasTimer(updateCheckTimer)) if (!updateCheckTimer) {
		timezone_call_remote_api();
	}
}

void timezone_update_check_enable() {
	bool enable = _Config.getOptions().isNTPClient();

	if_debug_printf_P(PSTR("NTP timezone update check: %s\n"), enable ? "Yes" : "No");

	timezone_remove_timer();
	if (enable) {
		add_wifi_event_callback(timezone_wifi_connected_callback, WIFI_CB_EVENT_CONNECTED);

		auto &config = _Config.get();
		configTime(0, 0, config.ntp.servers[0], config.ntp.servers[1], config.ntp.servers[2]);
		timezone_update_failures = 0;
		timezone_call_remote_api();

	} else {
		const char *str = _sharedEmptyString.c_str();
		configTime(0, 0, str, str, str);

		remove_wifi_event_callback(timezone_wifi_connected_callback, WIFI_CB_EVENT_CONNECTED);
	}
}

#endif


RemoteTimezone::RemoteTimezone() {
	_callback = nullptr;
	_zoneEnd = 0;
#if TIMEZONE_USE_HTTP_CLIENT == 0
	_httpClient = nullptr;
#endif
}
RemoteTimezone::~RemoteTimezone() {
#if TIMEZONE_USE_HTTP_CLIENT == 0
	delete _httpClient;
#endif
}

void RemoteTimezone::setUrl(const String url) {
	_url = url;
}

void RemoteTimezone::setTimezone(const String zoneName) {
	_zoneName = zoneName;
}

void RemoteTimezone::setStatusCallback(RemoteTimezoneStatusCallback_t callback) {
	_callback = callback;
}

void RemoteTimezone::get() {

	_zoneEnd = 0;

	String url = _url;
	if (_zoneName.length()) {
		url.replace(F("${timezone}"), _zoneName);
	}

	debug_printf_P(PSTR("RemoteTimezone:get(): URL %s\n"), url.c_str());

#if TIMEZONE_USE_HTTP_CLIENT
	HTTPClient http;

	http.begin(url);
	int httpCode = http.GET();
	if (httpCode == 200) {
		Stream &stream = http.getStream();
		_responseHandler(url, stream);
	} else {
		debug_printf_P(PSTR("RemoteTimezone error HTTP code %d\n"), httpCode);

		if (_callback) {
			PrintString message;
			message.printf_P(PSTR("HTTP error, code %d"), httpCode);
			_callback(false, message, 0);
		}
	}
	http.end();
#else

	if (_httpClient != nullptr) {
		delete _httpClient;
	}

	_httpClient = new asyncHTTPrequest();
	_httpClient->onReadyStateChange([url, this](void *, asyncHTTPrequest *request, int readyState) {
		if (readyState == 4) {
			int httpCode;
			if ((httpCode = request->responseHTTPcode()) == 200) {
				String response = request->responseText();
				BufferStream stream;

				stream.write(response.c_str());
				_responseHandler(url, stream);

			} else {
				debug_printf_P(PSTR("RemoteTimezone error HTTP code %d\n"), httpCode);

				PrintString message;
				message.printf_P(PSTR("HTTP error, code %d"), httpCode);
				if (_callback) {
					_callback(false, message, 0);
				}
			}
		}
	});
	_httpClient->setTimeout(30);

	debug_printf_P(PSTR("timezone remote api %s\n"), url.c_str());

	if (!_httpClient->open("GET", url.c_str())) {
		if (_callback) {
			_callback(false, F("HTTP Client: open() failed"), 0);
		}
	} else if (!_httpClient->send()) {
		if (_callback) {
			_callback(false, F("HTTP Client: send() failed"), 0);
		}
	}
#endif
}

time_t RemoteTimezone::getZoneEnd() {
	return _zoneEnd;
}

void RemoteTimezone::_responseHandler(const String url, Stream &stream) {

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

	JsonCallbackReader json = JsonCallbackReader(stream, [&args](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) -> bool {
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

			debug_printf_P(PSTR("RemoteTimezone response zoneName %s abbreviation %s DST %d offset %ld zone end %lu\n"), args.zoneName.c_str(), args.abbreviation.c_str(), args.dst, (long)args.offset, (unsigned long)args.zoneEnd);

			tz.setAbbreviation(args.abbreviation);
			tz.setTimezone(0, args.zoneName);
			tz.setDst(args.dst);
			tz.setOffset(args.offset);
		} else {
			debug_printf_P(PSTR("RemoteTimezone error: %s\n"), args.message.c_str());
		}
	} else {
		args.status = false;
		args.message = json.getLastErrorMessage();

		debug_printf_P(PSTR("RemoteTimezone JSON parser error: %s\n"), json.getLastErrorMessage().c_str());
	}

	if (args.zoneEnd <= 0) {  // data not available, set to one day in the future
		args.zoneEnd = time(nullptr) + 86400;
	}
	_zoneEnd = args.zoneEnd;

	if (_callback) {
		_callback(args.status, args.message, args.zoneEnd);
	}
}
