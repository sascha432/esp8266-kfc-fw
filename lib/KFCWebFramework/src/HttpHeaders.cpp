/**
 * Author: sascha_lammers@gmx.de
 */

#include <Timezone.h>
#include <algorithm>
#include <PrintString.h>
#include "HttpHeaders.h"
#if HAVE_HTTPHEADERS_ASYNCWEBSERVER
#include <ESPAsyncWebServer.h>
#endif

PROGMEM_STRING_DEF(Pragma, "Pragma");
PROGMEM_STRING_DEF(Link, "Link");
PROGMEM_STRING_DEF(Location, "Location");
PROGMEM_STRING_DEF(RFC7231_date, "%a, %d %b %Y %H:%M:%S GMT");
PROGMEM_STRING_DEF(Cache_Control, "Cache-Control");
PROGMEM_STRING_DEF(Content_Length, "Content-Length");
PROGMEM_STRING_DEF(Content_Encoding, "Content-Encoding");
PROGMEM_STRING_DEF(Connection, "Connection");
PROGMEM_STRING_DEF(Cookie, "Cookie");
PROGMEM_STRING_DEF(Set_Cookie, "Set-Cookie");
PROGMEM_STRING_DEF(Last_Modified, "Last-Modified");
PROGMEM_STRING_DEF(Expires, "Expires");
PROGMEM_STRING_DEF(no_cache, "no-cache");
PROGMEM_STRING_DEF(close, "close");
PROGMEM_STRING_DEF(keep_alive, "keep-alive");
PROGMEM_STRING_DEF(public, "public");
PROGMEM_STRING_DEF(private, "private");
PROGMEM_STRING_DEF(comma_, ", ");

HttpHeaders::HttpHeaders() {
    init();
}

HttpHeaders::HttpHeaders(bool addDefault) {
    if (addDefault) {
        init();
    }
}

HttpHeaders::~HttpHeaders() {
    clear(-1);
}

const String  HttpHeaders::getRFC7231Date(const time_t *time) {
    char buf[32];
#if HAS_STRFTIME_P
    strftime_P(buf, sizeof(buf), SPGM(RFC7231_date), gmtime(time));
#else
    char date_format[constexpr_strlen_P(SPGM(RFC7231_date)) + 1];
    strcpy_P(date_format, SPGM(RFC7231_date));
    strftime(buf, sizeof(buf), date_format, gmtime(time));
#endif
    return buf;
}


HttpHeader::HttpHeader(const String &name) {
    _name = name;
}

HttpHeader::~HttpHeader() {
}

const String HttpHeader::getHeader() {
    String tmp = getName();
    tmp += F(": ");
    tmp += getValue();
    return tmp;
    // return getName() + F(": ") + getValue();
}

bool HttpHeader::equals(const HttpHeader &header) const {
    return _name.equalsIgnoreCase(header.getName());
}

HttpPragmaHeader::HttpPragmaHeader(const String &value) : HttpSimpleHeader(FSPGM(Pragma), value) {
}

HttpDispositionHeader::HttpDispositionHeader(const String& filename) : HttpSimpleHeader(F("Content-Disposition"))
{
    String str = F("attachment; filename=\"");
    str += filename;
    str += '"';
    setHeader(str);
}

HttpLocationHeader::HttpLocationHeader(const String &location) : HttpSimpleHeader(FSPGM(Location), location) {
}


HttpLinkHeader::HttpLinkHeader(const String &location) : HttpSimpleHeader(FSPGM(Link), location) {
}

HttpDateHeader::HttpDateHeader(const String &name, const String &expires) : HttpSimpleHeader(name, expires) {
}

/* expires in seconds or Unix time */
HttpDateHeader::HttpDateHeader(const String &name, time_t expires) : HttpSimpleHeader(name) {
#if NTP_CLIENT || RTC_SUPPORT
    if (expires < 31536000) {
        if (time(nullptr) == 0) {
            expires = 0x7ffffff0;
        } else {
            expires += time(nullptr);
        }
    }
#else
    expires = 0x7ffffff0;
#endif
    HttpSimpleHeader::setHeader(HttpHeaders::getRFC7231Date(&expires));
}

HttpCacheControlHeader::HttpCacheControlHeader() : HttpHeader(FSPGM(Cache_Control)) {
    _noCache = false;
    _noStore = false;
    _mustRevalidate = false;
    _publicOrPrivate = CACHE_CONTROL_NONE;
    _maxAge = MAX_AGE_AUTO; // MAX_AGE_NOT_SET
}

const String HttpCacheControlHeader::getValue() const {
    String tmp;
    uint32_t maxAge = _maxAge;

    if (maxAge == MAX_AGE_AUTO && !_noCache) {
        maxAge = 31536000;
    }

    if (_noCache) {
        tmp = FSPGM(no_cache);
    }
    if (_noStore) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += F("no-store");
    }
    if (_mustRevalidate) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += F("must-revalidate");
    }
    if (_publicOrPrivate == CACHE_CONTROL_PRIVATE) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += FSPGM(private);
    }
    if (_publicOrPrivate == CACHE_CONTROL_PUBLIC) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += FSPGM(public);
    }
    if (maxAge != MAX_AGE_AUTO && maxAge != MAX_AGE_NOT_SET) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += F("max-age=");
        tmp += String(maxAge);
    }

    // debug_printf_P(PSTR("Cache-Control: %s\n"), tmp.c_str());
    return tmp;
}

HttpCookieHeader::HttpCookieHeader(const String &name) : HttpHeader(FSPGM(Set_Cookie)) {
    _cookieName = name;
    _expires = COOKIE_SESSION;
#if HTTP_COOKIE_MAX_AGE_SUPPORT
    _maxAge = MAX_AGE_NOT_SET;
#endif
}

const String  HttpCookieHeader::getValue() const {
    PrintString header;
    header.print(_cookieName);
    header += '=';
    if (_expires != COOKIE_EXPIRED) {
        header += _value;
    }
    if (_path.length()) {
        header += F("; Path=");
        header +=_path;
    }
    if (_expires != COOKIE_SESSION) {
        header += F("; Expires=");
        header += HttpHeaders::getRFC7231Date(&_expires);
    }
#if HTTP_COOKIE_MAX_AGE_SUPPORT
    if (_maxAge != MAX_AGE_NOT_SET) {
        header.printf_P(PSTR("; Max-Age=%d"), _maxAge);
    }
#endif
    return header;
}

#if HAVE_HTTPHEADERS_ASYNCWEBSERVER
bool  HttpCookieHeader::parseCookie(AsyncWebServerRequest *request, const String &name, String &value) {
    AsyncWebHeader *cookies;
    if ((cookies = request->getHeader(FSPGM(Cookie))) == nullptr) {
        return false;
    }
    return parseCookie(cookies->value(), name, value);
}
#endif

bool HttpCookieHeader::parseCookie(const String &cookies, const String &name, String &value) {

    int start = 0, end = 0;
    do {
        String cookie;
        int equals;
        end = cookies.indexOf(';', end + 1);
        cookie = cookies.substring(start, end == -1 ? cookies.length() : end);
        equals = cookie.indexOf('=');
        if (equals != -1) {
            if (cookie.substring(0, equals) == name) {
                value = cookies.substring(equals + 1, end);
                return true;
            }
        }
        if (end == -1) {
            break;
        }
        while (cookies.charAt(++end) == ' ') {
        }
        start = end;

    } while(true);

    return false;
}

HttpConnectionHeader::HttpConnectionHeader(uint8_t type) : HttpSimpleHeader(FSPGM(Connection), type == HTTP_CONNECTION_CLOSE ? FSPGM(close) : FSPGM(keep_alive)) {
}

void  HttpHeaders::clear(uint8_t reserveItems) {
    _headers.clear();
    _headers.reserve(reserveItems);
}

void  HttpHeaders::init() {
    clear(5);
    addDefaultHeaders();
}

void  HttpHeaders::add(const String &name, const String &value) {
    add(new HttpSimpleHeader(name, value));
}

HttpHeadersVector  &HttpHeaders::getHeaders() {
    return _headers;
}

HttpHeadersIterator  HttpHeaders::begin() {
    return _headers.begin();
}

HttpHeadersIterator  HttpHeaders::end() {
    return _headers.end();
}

void HttpHeaders::addNoCache(bool noStore)
{
    replace(new HttpPragmaHeader(FSPGM(no_cache)));
    auto hdr = new HttpCacheControlHeader();
    hdr->setPrivate().setNoCache(true).setNoStore(noStore).setMustRevalidate(noStore);
    replace(hdr);
    remove(FSPGM(Expires));
    remove(FSPGM(Last_Modified));
}

void HttpHeaders::addDefaultHeaders()
{
    add(F("Access-Control-Allow-Origin"), F("*"));
    add(new HttpLinkHeader(F("<https://fonts.gstatic.com>; rel=preconnect; crossorigin")));
    add(F("X-Frame-Options"), F("SAMEORIGIN"));
    add(new HttpDateHeader(FSPGM(Expires), 86400 * 30));
}

void HttpHeaders::setHeadersCallback(SetCallback_t callback, bool doClear)
{
    if (_headers.size()) {
        for (const auto &header : _headers) {
            callback(header->getName(), header->getValue());
        }
        if (doClear) {
            clear(0);
        }
    }
}

#if HAVE_HTTPHEADERS_ASYNCWEBSERVER

void HttpHeaders::setWebServerResponseHeaders(AsyncWebServerResponse *response)
{
    setHeadersCallback([response](const String &name, const String &header) {
        response->addHeader(name, header);
    }, true);
}

#endif

#if DEBUG

void  HttpHeaders::dump(Print &output) {
    output.printf_P(PSTR("--- %d\n"), _headers.size());
    for (const auto &header : _headers) {
        output.println(header->getHeader());
    }
}

#endif

HttpHeadersCmpFunction HttpHeaders::compareName(const String &name) {
    return [&name](const HttpHeaderPtr &_header) {
        return _header->getName().equalsIgnoreCase(name);
    };
}

HttpHeadersCmpFunction  HttpHeaders::compareHeader(const HttpHeader &header) {
    return [&header](const HttpHeaderPtr &_header) {
        return _header->equals(header);
    };
}
