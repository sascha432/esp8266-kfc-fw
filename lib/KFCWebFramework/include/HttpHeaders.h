/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <time.h>
#include <functional>
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
PROGMEM_STRING_DECL(comma_);

#ifndef HAVE_HTTPHEADERS_ASYNCWEBSERVER
#define HAVE_HTTPHEADERS_ASYNCWEBSERVER 1
#endif

#if HAVE_HTTPHEADERS_ASYNCWEBSERVER

class AsyncWebServerResponse;
class AsyncWebServerRequest;

#endif

class HttpHeader;
class Httpheaders;

typedef std::shared_ptr<HttpHeader>                             HttpHeaderPtr;
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
    virtual const String getHeader();

    virtual bool equals(const HttpHeaderPtr &header) const;

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

private:
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
    enum {
        HTTP_CONNECTION_CLOSE = 1,
        HTTP_CONNECTION_KEEP_ALIVE = 0,
    };

public:
    HttpConnectionHeader(uint8_t type = HTTP_CONNECTION_CLOSE);
};

class HttpDateHeader : public HttpSimpleHeader {
public:
    HttpDateHeader(const String &name, const String &expires);
    HttpDateHeader(const String &name, time_t expires);
};

class HttpContentLengthHeader : public HttpHeader {
public:
    HttpContentLengthHeader(size_t size) : HttpHeader(FSPGM(Content_Length)) {
        _size = size;
    }

    virtual const String getValue() const override {
        return String(_size);
    }
private:
    size_t _size;
};

class HttpCacheControlHeader : public HttpHeader {
public:
    enum {
        CACHE_CONTROL_NONE = 0,
        CACHE_CONTROL_PRIVATE,
        CACHE_CONTROL_PUBLIC,
    };

    enum {
        MAX_AGE_AUTO = 0xffffffff,
        MAX_AGE_NOT_SET = 0,
    };

    HttpCacheControlHeader();

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
        _publicOrPrivate = CACHE_CONTROL_PRIVATE;
        return *this;
    }
    HttpCacheControlHeader &setPublic() {
        _publicOrPrivate = CACHE_CONTROL_PUBLIC;
        _noCache = false;
        return *this;
    }
    HttpCacheControlHeader &setMaxAge(uint32_t maxAge) {
        _maxAge = maxAge;
        if (_maxAge != MAX_AGE_NOT_SET && _maxAge != MAX_AGE_AUTO) {
            setNoCache(false);
        }
        return *this;
    }

    virtual const String getValue() const override;
private:
    uint8_t _noCache: 1;
    uint8_t _noStore: 1;
    uint8_t _mustRevalidate: 1;
    uint8_t _publicOrPrivate: 2;
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

    HttpCookieHeader(const String &name);
    HttpCookieHeader(const String &name, const String &value) : HttpCookieHeader(name) {
        _value = value;
    }
    HttpCookieHeader(const String &name, const String &value, const String &path) : HttpCookieHeader(name) {
        _value = value;
        _path = path;
    }
    HttpCookieHeader(const String &name, const String &value, const String &path, time_t expires) : HttpCookieHeader(name) {
        _value = value;
        _path = path;
        _expires = expires;
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

    HttpCookieHeader &hasExpired() {
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

    virtual bool equals(const HttpHeaderPtr &header) const override {
        return equals(static_cast<HttpCookieHeader&>(*header));
    }

    bool equals(const HttpCookieHeader &header) const {
        if (_name.equalsIgnoreCase(header._name) && _cookieName.equals(header._cookieName) && _path.equals(header._path)) { // domain,secure,http only
            return true;
        }
        return false;
    }

    virtual const String getValue() const override;

#if HAVE_HTTPHEADERS_ASYNCWEBSERVER
    static bool parseCookie(AsyncWebServerRequest *request, const String &name, String &value);
#endif
    static bool parseCookie(const String &cookies, const String &name, String &value);
private:
    String _cookieName;
    String _value;
    String _path;
    // String _domain;
    //bool _secure;
    //bool _httpOnly;
#if HTTP_COOKIE_MAX_AGE_SUPPORT
    int32_t _maxAge;
#endif
    time_t _expires;
};



class HttpHeaders {
public:
    HttpHeaders();
    HttpHeaders(bool addDefault);
    virtual ~HttpHeaders();

    static const String getRFC7231Date(const time_t *time);
    static HttpHeaders &getInstance();

    void init();
    void clear(uint8_t reserveItems = 0);

    template<typename T, typename std::enable_if<std::is_base_of<HttpHeader, T>::value>::type* = nullptr>
    void add(T header) {
        _headers.push_back(std::make_shared<T>(header));
    }

    template<typename T, typename std::enable_if<std::is_base_of<HttpHeader, T>::value>::type* = nullptr>
    void replace(T header) {
        const HttpHeaderPtr &headerPtr = std::make_shared<T>(header);
        remove(headerPtr);
        _headers.push_back(headerPtr);
    }

    template<typename T, typename std::enable_if<std::is_base_of<HttpHeader, T>::value>::type* = nullptr>
    void remove(const T &header) {
        _headers.erase(std::remove_if(_headers.begin(), _headers.end(), compareHeaderPtr(std::make_shared<T>(header))), _headers.end());
    }

    void add(const String &name, const String &value);

    void remove(const String &name) {
        _headers.erase(std::remove_if(_headers.begin(), _headers.end(), compareName(name)), _headers.end());
    }
    void remove(const HttpHeaderPtr &header) {
        _headers.erase(std::remove_if(_headers.begin(), _headers.end(), compareHeaderPtr(header)), _headers.end());
    }

    HttpHeadersVector &getHeaders();
    HttpHeadersIterator begin();
    HttpHeadersIterator end();

    void addNoCache(bool noStore = false);
    void addDefaultHeaders();
#if HAVE_HTTPHEADERS_ASYNCWEBSERVER
    void setWebServerResponseHeaders(AsyncWebServerResponse *response);
#endif

#if DEBUG
    void dump(Print &output);
#endif

private:
    HttpHeadersCmpFunction compareName(const String &name);
    HttpHeadersCmpFunction compareHeaderPtr(const HttpHeaderPtr &headerPtr);

    static HttpHeaders _instance;

    HttpHeadersVector _headers;
};
