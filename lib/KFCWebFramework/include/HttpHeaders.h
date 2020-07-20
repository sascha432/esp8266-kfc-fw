/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "../src/generated/FlashStringGeneratorAuto.h"
#include <time.h>
#include <functional>
#include <memory>
#include <vector>

PROGMEM_STRING_DECL(Pragma);
PROGMEM_STRING_DECL(Link);
PROGMEM_STRING_DECL(Location);
PROGMEM_STRING_DECL(RFC7231_date);
PROGMEM_STRING_DECL(Cache_Control);
PROGMEM_STRING_DECL(Content_Length);
PROGMEM_STRING_DECL(Content_Encoding);
PROGMEM_STRING_DECL(Connection);
PROGMEM_STRING_DECL(Cookie);
PROGMEM_STRING_DECL(Set_Cookie);
PROGMEM_STRING_DECL(Last_Modified);
PROGMEM_STRING_DECL(Expires);
PROGMEM_STRING_DECL(no_cache);
PROGMEM_STRING_DECL(public);
PROGMEM_STRING_DECL(private);
PROGMEM_STRING_DECL(close);
PROGMEM_STRING_DECL(keep_alive);
PROGMEM_STRING_DECL(Authorization);
PROGMEM_STRING_DECL(Bearer_);

#ifndef HAVE_HTTPHEADERS_ASYNCWEBSERVER
#define HAVE_HTTPHEADERS_ASYNCWEBSERVER 1
#endif

#if HAVE_HTTPHEADERS_ASYNCWEBSERVER

class AsyncWebServerResponse;
class AsyncWebServerRequest;
class AsyncBaseResponse;

#endif

class HttpHeader;
class Httpheaders;

typedef std::unique_ptr<HttpHeader>                             HttpHeaderPtr;
typedef std::vector<HttpHeaderPtr>                              HttpHeadersVector;
typedef std::vector<HttpHeaderPtr>::iterator                    HttpHeadersIterator;
typedef std::function<bool(const HttpHeaderPtr &_header)>       HttpHeadersCmpFunction;

class HttpHeader {
public:
    HttpHeader(const String &name);
    virtual ~HttpHeader();

    virtual const String &getName() const {
        return _name;
    }
    virtual const String getValue() const {
        return String();
    }
    //virtual const String getHeader();
    void printTo(Print &output) const;

    virtual bool equals(const HttpHeader &header) const;

protected:
    String _name;
};

class HttpSimpleHeader : public HttpHeader {
public:
    HttpSimpleHeader(const String &name) : HttpHeader(name) {
    }
    HttpSimpleHeader(const String &name, const String &header) : HttpHeader(name) {
        _header = header;
    }

    void setHeader(const String &header) {
        _header = header;
    }

    virtual const String getValue() const override {
        return _header;
    }

protected:
    String _header;
};

class HttpDispositionHeader : public HttpSimpleHeader {
public:
    HttpDispositionHeader(const String& filename);
};

class HttpPragmaHeader : public HttpSimpleHeader {
public:
    HttpPragmaHeader(const String &value);
};

class HttpLocationHeader : public HttpSimpleHeader {
public:
    HttpLocationHeader(const String &location);
};

class HttpLinkHeader : public HttpSimpleHeader {
public:
    HttpLinkHeader(const String &location);
};

class HttpConnectionHeader : public HttpSimpleHeader {
public:
    typedef enum {
        CLOSE = 1,
        KEEP_ALIVE = 0,
    } ConnectionEnum_t;

public:
    HttpConnectionHeader(ConnectionEnum_t type = CLOSE);
};

class HttpDateHeader : public HttpSimpleHeader {
public:
    HttpDateHeader(const String &name, const String &expires);
    HttpDateHeader(const String &name, time_t expires);
};

class HttpContentLengthHeader : public HttpHeader {
public:
    HttpContentLengthHeader(size_t size) : HttpHeader(FSPGM(Content_Length)), _size(size) {
    }

    virtual const String getValue() const override {
        return String(_size);
    }

private:
    size_t _size;
};

class HttpCacheControlHeader : public HttpHeader {
public:
    typedef enum {
        NONE = 0,
        PRIVATE,
        PUBLIC,
    } CacheControlEnum_t;

    typedef enum {
        AUTO = 0xffffffff,
        NOT_SET = 0,
    } MaxAgeEnum_t;

    HttpCacheControlHeader(CacheControlEnum_t cacheControl = NONE, bool mustRevalidate = false, bool noCache = false, bool noStore = false) :
        HttpHeader(FSPGM(Cache_Control)),
        _noCache(noCache),
        _noStore(noStore),
        _mustRevalidate(mustRevalidate),
        _cacheControl(cacheControl)
    {
        setMaxAge(AUTO);
    }

    HttpCacheControlHeader &setNoCache(bool noCache) {
        _noCache = noCache;
        return *this;
    }
    HttpCacheControlHeader &setNoStore(bool noStore) {
        _noStore = noStore;
        return *this;
    }
    HttpCacheControlHeader &setMustRevalidate(bool mustRevalidate) {
        _mustRevalidate = mustRevalidate;
        return *this;
    }
    HttpCacheControlHeader &setPrivate() {
        _cacheControl = PRIVATE;
        return *this;
    }
    HttpCacheControlHeader &setPublic() {
        _cacheControl = PUBLIC;
        _noCache = false;
        return *this;
    }
    HttpCacheControlHeader &setMaxAge(uint32_t maxAge) {
        _maxAge = maxAge;
        if (_maxAge != NOT_SET && _maxAge != AUTO) {
            setNoCache(false);
        }
        return *this;
    }

    virtual const String getValue() const override;

private:
    uint8_t _noCache: 1;
    uint8_t _noStore: 1;
    uint8_t _mustRevalidate: 1;
    uint8_t _cacheControl: 2;
    uint32_t _maxAge;
};

class HttpCookieHeader : public HttpHeader {
public:
    enum CookieType {
        COOKIE_SESSION = 0,
        COOKIE_EXPIRED = 1000000000,
    };

#if HTTP_COOKIE_MAX_AGE_SUPPORT
    enum {
        MAX_AGE_NOT_SET = 0U,
    };
#endif

    HttpCookieHeader(const String &name, const String &value = String(), const String &path = String(), time_t expires = COOKIE_SESSION) : HttpHeader(FSPGM(Set_Cookie)), _cookieName(name), _value(value), _path(path) {
        setExpires(expires);
    }

    HttpCookieHeader &setName(const String &name) {
        _cookieName = name;
        return *this;
    }

    HttpCookieHeader &setName(const __FlashStringHelper *name) {
        _cookieName = name;
        return *this;
    }

    HttpCookieHeader &setPath(const String &path) {
        _path = path;
        return *this;
    }

    HttpCookieHeader &setPath(const __FlashStringHelper *path) {
        _path = path;
        return *this;
    }

    HttpCookieHeader &setExpires(time_t expires) {
        _expires = expires;
#if HTTP_COOKIE_MAX_AGE_SUPPORT
        if (expires == COOKIE_EXPIRED && _maxAge != MAX_AGE_NOT_SET) {
            _maxAge = 0;
        }
#endif
        return *this;
    }

#if HTTP_COOKIE_MAX_AGE_SUPPORT
    HttpCookieHeader &setMaxAge(int32_t maxAge) {
        _maxAge = maxAge;
        if (_maxAge <= 0) {
            _expires = COOKIE_EXPIRED;
        } else {
            time_t t;
            if (IS_TIME_VALID(t = time(nullptr))) {
                _expires = t + maxAge;
            }
        }
        return *this;
    }
#endif

    HttpCookieHeader &setHasExpired() {
        _expires = COOKIE_EXPIRED;
#if HTTP_COOKIE_MAX_AGE_SUPPORT
        _maxAge = MAX_AGE_NOT_SET;
#endif
        return *this;
    }

    HttpCookieHeader &setValue(const String &value) {
        _value = value;
        return *this;
    }

    virtual bool equals(const HttpHeader &header) const override
    {
        return equals(static_cast<const HttpCookieHeader&>(header));
    }

    bool equals(const HttpCookieHeader &header) const {
        if (_name.equalsIgnoreCase(header._name) && _cookieName.equals(header._cookieName) && _path.equals(header._path)) { // domain,secure,http only
            return true;
        }
        return false;
    }

    virtual const String getValue() const override;

    // NOTE for parseCookie: max. length of name is 255 characters
    // value is not modified if the function returns false
#if HAVE_HTTPHEADERS_ASYNCWEBSERVER
    static bool parseCookie(AsyncWebServerRequest *request, const String &name, String &value);
#endif
    static bool parseCookie(const String &cookies, const String &name, String &value);
private:
    String _cookieName;
    String _value;
    String _path;
    time_t _expires;
    // String _domain;
    //bool _secure;
    //bool _httpOnly;
#if HTTP_COOKIE_MAX_AGE_SUPPORT
    int32_t _maxAge;
#endif
};

class HttpAuthorization : public HttpSimpleHeader {
public:
    typedef enum {
        BASIC = 0,
        BEARER = 1,
    } AuthorizationTypeEnum_t;

    HttpAuthorization(const String &auth = String()) : HttpSimpleHeader(FSPGM(Authorization), auth) {
    }
    HttpAuthorization(AuthorizationTypeEnum_t type, const String &auth) : HttpAuthorization(auth) {
        switch(type) {
            case BASIC:
                setHeader(F("Basic "));
                break;
            case BEARER:
                setHeader(FSPGM(Bearer_));
                break;
        }
        _header += auth;
    }
};

class HttpContentType : public HttpSimpleHeader {
public:
    HttpContentType(const String &type, const String &charset = String()) : HttpSimpleHeader(F("Content-Type"), type) {
        if (charset.length()) {
            _header += F("; charset=");
            _header += charset;
        }
    }
};

class HttpHeaders {
public:
    typedef std::function<void(const String &name, const String &header)> SetCallback_t;

    HttpHeaders();
    HttpHeaders(bool addDefault);
    virtual ~HttpHeaders();

    static const String getRFC7231Date(const time_t *time);
    static HttpHeaders &getInstance();

    void init();
    void clear(uint8_t reserveItems = 0);

    void add(HttpHeader *header) {
        _headers.emplace_back(header);
    }

    void add(const String& name, const String& value);

    void replace(HttpHeader *header) {
        remove(*header);
        _headers.emplace_back(header);
    }

    void remove(const String &name) {
        _headers.erase(std::remove_if(_headers.begin(), _headers.end(), compareName(name)), _headers.end());
    }
    void remove(const HttpHeader& header) {
        _headers.erase(std::remove_if(_headers.begin(), _headers.end(), compareHeader(header)), _headers.end());
    }

    HttpHeadersVector &getHeaders();
    HttpHeadersIterator begin();
    HttpHeadersIterator end();

    void addNoCache(bool noStore = false);
    void addDefaultHeaders();
    void setHeadersCallback(SetCallback_t callback, bool doClear);
#if HAVE_HTTPHEADERS_ASYNCWEBSERVER
    void setAsyncWebServerResponseHeaders(AsyncWebServerResponse *response);

    template <class __AsyncBaseResponse>
    void setAsyncBaseResponseHeaders(__AsyncBaseResponse *response)
    {
        response->_httpHeaders = std::move(_headers);
    }
#endif

    void printTo(Print &output);
#if DEBUG
    void dump(Print &output);
#endif

private:
    HttpHeadersCmpFunction compareName(const String &name);
    HttpHeadersCmpFunction compareHeader(const HttpHeader &headerPtr);

    static HttpHeaders _instance;

    HttpHeadersVector _headers;
};
