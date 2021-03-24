/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include <PrintString.h>
#include "HttpHeaders.h"
#if HAVE_HTTPHEADERS_ASYNCWEBSERVER
#include <ESPAsyncWebServer.h>
#endif

FLASH_STRING_GENERATOR_AUTO_INIT(
    AUTO_STRING_DEF(Pragma, "Pragma")
    AUTO_STRING_DEF(Link, "Link")
    AUTO_STRING_DEF(Location, "Location")
    AUTO_STRING_DEF(RFC7231_date, "%a, %d %b %Y %H:%M:%S GMT")
    AUTO_STRING_DEF(Cache_Control, "Cache-Control")
    AUTO_STRING_DEF(Content_Length, "Content-Length")
    AUTO_STRING_DEF(Content_Encoding, "Content-Encoding")
    AUTO_STRING_DEF(Connection, "Connection")
    AUTO_STRING_DEF(Cookie, "Cookie")
    AUTO_STRING_DEF(Set_Cookie, "Set-Cookie")
    AUTO_STRING_DEF(Last_Modified, "Last-Modified")
    AUTO_STRING_DEF(Expires, "Expires")
    AUTO_STRING_DEF(no_cache, "no-cache")
    AUTO_STRING_DEF(close, "close")
    AUTO_STRING_DEF(keep_alive, "keep-alive")
    AUTO_STRING_DEF(public, "public")
    AUTO_STRING_DEF(private, "private")
    AUTO_STRING_DEF(Authorization, "Authorization")
    AUTO_STRING_DEF(Bearer_, "Bearer ")
);

HttpHeaders::HttpHeaders()
{
    init();
}

HttpHeaders::HttpHeaders(bool addDefault)
{
    if (addDefault) {
        init();
    }
}


HttpHeaders::~HttpHeaders()
{
    clear(-1);
}

const String  HttpHeaders::getRFC7231Date(const time_t *time)
{
    char buf[32];
#if HAS_STRFTIME_P
    strftime_P(buf, sizeof(buf), SPGM(RFC7231_date), gmtime(time));
#elif _MSC_VER
    strftime(buf, sizeof(buf), String(FSPGM(RFC7231_date)).c_str(), gmtime(time));
#else
    char date_format[constexpr_strlen_P(SPGM(RFC7231_date)) + 1];
    strcpy_P(date_format, SPGM(RFC7231_date));
    strftime(buf, sizeof(buf), date_format, gmtime(time));
#endif
    return buf;
}


HttpHeader::HttpHeader(const String &name)
{
    _name = name;
}

HttpHeader::~HttpHeader()
{
}

// const String HttpHeader::getHeader()
// {
//     String tmp = getName();
//     tmp += F(": ");
//     tmp += getValue();
//     return tmp;
// }

void HttpHeader::printTo(Print &output) const
{
    output.print(getName());
    output.print(F(": "));
    output.println(getValue());
}

bool HttpHeader::equals(const HttpHeader &header) const {
    return _name.equalsIgnoreCase(header.getName());
}

HttpPragmaHeader::HttpPragmaHeader(const String &value) : HttpSimpleHeader(FSPGM(Pragma), value)
{
}

HttpDispositionHeader::HttpDispositionHeader(const String& filename) : HttpSimpleHeader(F("Content-Disposition"))
{
    String str = F("attachment; filename=\"");
    str += filename;
    str += '"';
    setHeader(str);
}

HttpLocationHeader::HttpLocationHeader(const String &location) : HttpSimpleHeader(FSPGM(Location), location)
{
}


HttpLinkHeader::HttpLinkHeader(const String &location) : HttpSimpleHeader(FSPGM(Link), location)
{
}

HttpDateHeader::HttpDateHeader(const String &name, const String &expires) : HttpSimpleHeader(name, expires)
{
}

/* expires in seconds or Unix time */
HttpDateHeader::HttpDateHeader(const String &name, time_t expires) : HttpSimpleHeader(name)
{
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

const String HttpCacheControlHeader::getValue() const
{
    String tmp;
    uint32_t maxAge = _maxAge;

    if (maxAge == AUTO && !_noCache) {
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
    if (_cacheControl == PRIVATE) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += FSPGM(private);
    }
    if (_cacheControl == PUBLIC) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += FSPGM(public);
    }
    if (maxAge != AUTO && maxAge != NOT_SET) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += F("max-age=");
        tmp += String(maxAge);
    }

    // debug_printf_P(PSTR("Cache-Control: %s\n"), tmp.c_str());
    return tmp;
}

const String HttpCookieHeader::getValue() const
{
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

bool HttpCookieHeader::parseCookie(AsyncWebServerRequest *request, const String &name, String &value)
{
    // find all cookie headers
    for(const auto header: request->getHeaders()) {
        // __DBG_printf("header.key=% val=%s", header->name().c_str(), header->value().c_str());
        if (header->name().equalsIgnoreCase(FSPGM(Cookie))) {
            // find name in this cookie collection
           if (parseCookie(header->value(), name, value)) {
               return true;
           }
        }
    }
    return false;
}

#endif

bool HttpCookieHeader::parseCookie(const String &cookies, const String &name, String &value)
{
    auto cCookies = cookies.c_str();
    uint8_t nameLength = (uint8_t)name.length();
    size_t start = 0;
    int end = 0;
    do {
        // trim leading whitespace
        auto ptr = &cCookies[start];
        while (*ptr && isspace(*ptr)) {
            ptr++;
            start++;
            end++;
        }
        if (!*ptr) { // trailing ;
            break;
        }
        // find end of this cookie
        end = cookies.indexOf(';', end + 1);
        // total length
        size_t len = (end == -1) ? (cookies.length() - start) : (end - start);
        if (len > nameLength) {
            if ((cCookies[start + nameLength] == '=') && !strncmp(name.c_str(), &cCookies[start], nameLength)) {
#if 1
                value = cookies.substring(start + (++nameLength), start + len);
#else
                // move start to the value after the name and shorten length
                nameLength++;
                len -= nameLength;
                start += nameLength;
                value = PrintString(reinterpret_cast<const uint8_t *>(&cCookies[start]), len);
#endif
                return true;
            }
        }
    } while ((start = ++end) != 0); // end is -1 for the last cookie

    return false;
}



void HttpHeaders::clear(uint8_t reserveItems)
{
    _headers.clear();
    _headers.reserve(reserveItems);
}

void HttpHeaders::init()
{
    clear(5);
    addDefaultHeaders();
}

void HttpHeaders::addNoCache(bool noStore)
{
    replace(new HttpPragmaHeader(FSPGM(no_cache)));
    replace(new HttpCacheControlHeader(HttpCacheControlHeader::PRIVATE, noStore, true, noStore));
    remove(FSPGM(Expires));
    remove(FSPGM(Last_Modified));
}

void HttpHeaders::addDefaultHeaders()
{
#if DEBUG_ASSETS
    add(F("Access-Control-Allow-Origin"), String('*'));
    add(new HttpLinkHeader(String("<http://") + WiFi.localIP().toString() + String(":80>; rel=preconnect; crossorigin")));
    add(new HttpLinkHeader(F("<" DEBUG_ASSETS_URL1 ">; rel=preconnect; crossorigin")));
    add(new HttpLinkHeader(F("<" DEBUG_ASSETS_URL2 ">; rel=preconnect; crossorigin")));
    add(new HttpLinkHeader(F("<https://fonts.gstatic.com>; rel=preconnect; crossorigin")));
#else
    add(F("Access-Control-Allow-Origin"), String('*'));
    add(new HttpLinkHeader(F("<https://fonts.gstatic.com>; rel=preconnect; crossorigin")));
    add(F("X-Frame-Options"), F("SAMEORIGIN"));
#endif
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

void HttpHeaders::_setAsyncWebServerResponseHeaders(AsyncWebServerResponse *response)
{
    setHeadersCallback([response](const String &name, const String &header) {
        // __DBG_printf("name=%s value=%s", __S(name), __S(header));
        response->addHeader(name, header);
    }, true);
}

#endif

void HttpHeaders::printTo(Print &output) const
{
    for (const auto &header : _headers) {
        header->printTo(output);
    }
}

#if DEBUG

void HttpHeaders::dump(Print &output) const
{
    output.printf_P(PSTR("--- %d\n"), _headers.size());
    printTo(output);
}

#endif

HttpHeadersCmpFunction HttpHeaders::compareName(const String &name) const
{
    return [&name](const HttpHeaderPtr &_header) {
        return _header->getName().equalsIgnoreCase(name);
    };
}

HttpHeadersCmpFunction  HttpHeaders::compareHeader(const HttpHeader &header) const
{
    return [&header](const HttpHeaderPtr &_header) {
        return _header->equals(header);
    };
}
