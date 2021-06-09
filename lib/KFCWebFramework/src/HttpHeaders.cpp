/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include <PrintString.h>
#include "HttpHeaders.h"
#if HAVE_HTTPHEADERS_ASYNCWEBSERVER
#include <ESPAsyncWebServer.h>
#endif

#include "../include/spgm_auto_def.h"
#include "../include/spgm_auto_strings.h"


String HttpHeaders::getRFC7231Date(const time_t *time)
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


// const String HttpHeader::getHeader()
// {
//     String tmp = getName();
//     tmp += F(": ");
//     tmp += getValue();
//     return tmp;
// }

HttpDispositionHeader::HttpDispositionHeader(const String& filename) :
    HttpSimpleHeader(F("Content-Disposition"))
{
    _header.reserve(23 + filename.length() + 1);
    _header += F("attachment; filename=\"");
    _header += filename;
    _header += '"';
}

AsyncWebServerResponse *HttpLocationHeader::redir(AsyncWebServerRequest *request, const String &url, HttpHeaders &headers)
{
    auto response = request->beginResponse(302);
    headers.replace<HttpLocationHeader>(url);
    headers.setResponseHeaders(response);
    return response;
}

AsyncWebServerResponse *HttpLocationHeader::redir(AsyncWebServerRequest *request, const String &url)
{
    HttpHeaders headers;
    headers.addNoCache(true);
    return redir(request, url, headers);
}


/* expires in seconds or Unix time */
HttpDateHeader::HttpDateHeader(const String &name, time_t expires) :
    HttpSimpleHeader(name)
{
    // treat everything less than 10 years as relative
    if (expires < static_cast<time_t>(MaxAgeType::TEN_YEARS) && isTimeValid()) {
            expires += time(nullptr);
    }
    else {
        // if the time is invald, use an arbitrary time in the future
        expires = TIME_T_MAX;
    }
    HttpSimpleHeader::setHeader(std::move(HttpHeaders::getRFC7231Date(&expires)));
}

const String HttpCacheControlHeader::getValue() const
{
    String tmp;
    auto maxAge = _maxAge;

    if (maxAge == MaxAgeType::AUTO && !_noCache) {
        maxAge = MaxAgeType::ONE_YEAR;
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
    if (static_cast<CacheControlType>(_cacheControl) == CacheControlType::PRIVATE) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += FSPGM(private);
    }
    if (static_cast<CacheControlType>(_cacheControl) == CacheControlType::PUBLIC) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += FSPGM(public);
    }
    if (maxAge != MaxAgeType::AUTO && maxAge != MaxAgeType::NOT_SET) {
        if (tmp.length()) {
            tmp += FSPGM(comma_);
        }
        tmp += F("max-age=");
        tmp += String(static_cast<std::underlying_type<MaxAgeType>::type>(maxAge));
    }

    // debug_printf_P(PSTR("Cache-Control: %s\n"), tmp.c_str());
    return tmp;
}

const String HttpCookieHeader::getValue() const
{
    String header = _cookieName;
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

void HttpHeaders::addNoCache(bool noStore)
{
    replace(new HttpPragmaHeader(FSPGM(no_cache)));
    replace(new HttpCacheControlHeader(HttpCacheControlHeader::CacheControlType::PRIVATE, noStore, true, noStore));
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

void HttpHeaders::setHeadersCallback(SetCallback callback, bool doClear)
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
