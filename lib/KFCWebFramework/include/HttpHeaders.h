/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <time.h>
#include <functional>
#include <memory>
#include <vector>

#ifndef HAVE_HTTPHEADERS_ASYNCWEBSERVER
#define HAVE_HTTPHEADERS_ASYNCWEBSERVER 1
#endif

#if HAVE_HTTPHEADERS_ASYNCWEBSERVER

class AsyncWebServerResponse;
class AsyncWebServerRequest;
class AsyncBaseResponse;

#endif

class HttpHeader;
class HttpHeaders;

typedef std::unique_ptr<HttpHeader>                             HttpHeaderPtr;
typedef std::vector<HttpHeaderPtr>                              HttpHeadersVector;
typedef std::vector<HttpHeaderPtr>::iterator                    HttpHeadersIterator;
typedef std::function<bool(const HttpHeaderPtr &_header)>       HttpHeadersCmpFunction;

// ------------------------------------------------------------------------
// HttpHeader
// ------------------------------------------------------------------------

class HttpHeader {
public:
    HttpHeader(const String &name);
    HttpHeader(String &&name);
    virtual ~HttpHeader();

    virtual const String &getName() const;
    virtual const String getValue() const;
    //virtual const String getHeader();
    virtual bool equals(const HttpHeader &header) const;

    void printTo(Print &output) const;

protected:
    String _name;
};

inline HttpHeader::HttpHeader(const String &name)
{
    _name = name;
}

inline HttpHeader::HttpHeader(String &&name)
{
    _name = std::move(name);
}

inline HttpHeader::~HttpHeader()
{
}

inline bool HttpHeader::equals(const HttpHeader &header) const
{
    return _name.equalsIgnoreCase(header.getName());
}

inline const String &HttpHeader::getName() const
{
    return _name;
}

inline const String HttpHeader::getValue() const
{
    return emptyString;
}

inline void HttpHeader::printTo(Print &output) const
{
    output.print(getName());
    output.print(F(": "));
    output.println(getValue());
}


// ------------------------------------------------------------------------
// HttpSimpleHeader
// ------------------------------------------------------------------------

class HttpSimpleHeader : public HttpHeader {
public:
    HttpSimpleHeader(const String &name);
    HttpSimpleHeader(const String &name, const String &header);

    void setHeader(const String &header);
    void setHeader(String &&header);

    virtual const String getValue() const override;

protected:
    String _header;
};

inline HttpSimpleHeader::HttpSimpleHeader(const String &name) :
    HttpHeader(name)
{
}

inline HttpSimpleHeader::HttpSimpleHeader(const String &name, const String &header) :
    HttpHeader(name), _header(header)
{
}

inline void HttpSimpleHeader::setHeader(const String &header)
{
    _header = header;
}

inline void HttpSimpleHeader::setHeader(String &&header)
{
    _header = std::move(header);
}

inline const String HttpSimpleHeader::getValue() const
{
    return _header;
}

// ------------------------------------------------------------------------
// HttpDispositionHeader
// ------------------------------------------------------------------------

class HttpDispositionHeader : public HttpSimpleHeader {
public:
    HttpDispositionHeader(const String& filename);
};

// ------------------------------------------------------------------------
// HttpPragmaHeader
// ------------------------------------------------------------------------

class HttpPragmaHeader : public HttpSimpleHeader {
public:
    HttpPragmaHeader(const String &value);
};

inline HttpPragmaHeader::HttpPragmaHeader(const String &value) :
    HttpSimpleHeader(FSPGM(Pragma), value)
{
}


// ------------------------------------------------------------------------
// HttpLocationHeader
// ------------------------------------------------------------------------

class HttpLocationHeader : public HttpSimpleHeader {
public:
    HttpLocationHeader(const String &location);

    static AsyncWebServerResponse *redir(AsyncWebServerRequest *request, const String &url, HttpHeaders &headers);
    static AsyncWebServerResponse *redir(AsyncWebServerRequest *request, const String &url);
};

inline HttpLocationHeader::HttpLocationHeader(const String &location) :
    HttpSimpleHeader(FSPGM(Location), location)
{
}

// ------------------------------------------------------------------------
// HttpLinkHeader
// ------------------------------------------------------------------------

class HttpLinkHeader : public HttpSimpleHeader {
public:
    HttpLinkHeader(const String &location);
};

inline HttpLinkHeader::HttpLinkHeader(const String &location) :
    HttpSimpleHeader(FSPGM(Link), location)
{
}

// ------------------------------------------------------------------------
// HttpConnectionHeader
// ------------------------------------------------------------------------

class HttpConnectionHeader : public HttpHeader {
public:
    enum class ConnectionType : uint8_t {
        CLOSE = 1,
        KEEP_ALIVE = 0,
    };

public:
    HttpConnectionHeader(ConnectionType type = ConnectionType::CLOSE);

    virtual const String getValue() const override;

private:
    ConnectionType _type;
};

inline HttpConnectionHeader::HttpConnectionHeader(ConnectionType type) :
    HttpHeader(FSPGM(Connection)), _type(type)
{
}

inline const String HttpConnectionHeader::getValue() const
{
    return _type == ConnectionType::CLOSE ? FSPGM(close) : FSPGM(keep_alive);
}

// ------------------------------------------------------------------------
// HttpDateHeader
// ------------------------------------------------------------------------

class HttpDateHeader : public HttpSimpleHeader {
public:
    enum class MaxAgeType : uint32_t {
        AUTO =  ~0U,
        NOT_SET = 0,
        ONE_YEAR = 365 * 86400,
    };

    HttpDateHeader(const String &name, const String &expires);
    HttpDateHeader(const String &name, time_t expires);
};

inline HttpDateHeader::HttpDateHeader(const String &name, const String &expires) :
    HttpSimpleHeader(name, expires)
{
}

// ------------------------------------------------------------------------
// HttpContentLengthHeader
// ------------------------------------------------------------------------

class HttpContentLengthHeader : public HttpHeader {
public:
    HttpContentLengthHeader(size_t size);

    virtual const String getValue() const override;

private:
    size_t _size;
};

inline HttpContentLengthHeader::HttpContentLengthHeader(size_t size) :
    HttpHeader(FSPGM(Content_Length)), _size(size)
{
}

inline const String HttpContentLengthHeader::getValue() const
{
    return String(_size);
}

// ------------------------------------------------------------------------
// HttpCacheControlHeader
// ------------------------------------------------------------------------

class HttpCacheControlHeader : public HttpHeader {
public:
    using MaxAgeType = HttpDateHeader::MaxAgeType;

    enum class CacheControlType : uint8_t {
        NONE = 0,
        PRIVATE,
        PUBLIC,
        MAX
    };
    static_assert(static_cast<std::underlying_type<CacheControlType>::type>(CacheControlType::MAX) <= 0b11, "integer truncated");

    HttpCacheControlHeader(CacheControlType cacheControl = CacheControlType::NONE, bool mustRevalidate = false, bool noCache = false, bool noStore = false);

    HttpCacheControlHeader &setNoCache(bool noCache);
    HttpCacheControlHeader &setNoStore(bool noStore);
    HttpCacheControlHeader &setMustRevalidate(bool mustRevalidate);
    HttpCacheControlHeader &setPrivate();
    HttpCacheControlHeader &setPublic();
    HttpCacheControlHeader &setMaxAge(uint32_t maxAge);
    HttpCacheControlHeader &setMaxAge(MaxAgeType maxAge);

    virtual const String getValue() const override;

private:
    bool _noCache;
    bool _noStore;
    bool _mustRevalidate;
    CacheControlType _cacheControl;
    MaxAgeType _maxAge;
};

static constexpr size_t kHttpCacheControlHeaderSize = sizeof(HttpCacheControlHeader);

inline HttpCacheControlHeader::HttpCacheControlHeader(CacheControlType cacheControl, bool mustRevalidate, bool noCache, bool noStore ) :
    HttpHeader(FSPGM(Cache_Control)),
    _noCache(noCache),
    _noStore(noStore),
    _mustRevalidate(mustRevalidate),
    _cacheControl(cacheControl)
{
    setMaxAge(MaxAgeType::AUTO);
}

inline HttpCacheControlHeader &HttpCacheControlHeader::setNoCache(bool noCache)
{
    _noCache = noCache;
    return *this;
}

inline HttpCacheControlHeader &HttpCacheControlHeader::setNoStore(bool noStore)
{
    _noStore = noStore;
    return *this;
}

inline HttpCacheControlHeader &HttpCacheControlHeader::setMustRevalidate(bool mustRevalidate)
{
    _mustRevalidate = mustRevalidate;
    return *this;
}

inline HttpCacheControlHeader &HttpCacheControlHeader::setPrivate()
{
    _cacheControl = CacheControlType::PRIVATE;
    return *this;
}

inline HttpCacheControlHeader &HttpCacheControlHeader::setPublic()
{
    _cacheControl = CacheControlType::PUBLIC;
    _noCache = false;
    return *this;
}

inline HttpCacheControlHeader &HttpCacheControlHeader::setMaxAge(uint32_t maxAge)
{
    return setMaxAge(static_cast<MaxAgeType>(maxAge));
}

inline HttpCacheControlHeader &HttpCacheControlHeader::setMaxAge(MaxAgeType maxAge)
{
    _maxAge = maxAge;
    if (_maxAge != MaxAgeType::NOT_SET && _maxAge != MaxAgeType::AUTO) {
        setNoCache(false);
    }
    return *this;
}

// ------------------------------------------------------------------------
// HttpCookieHeader
// ------------------------------------------------------------------------

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

    HttpCookieHeader(const String &name, const String &value = String(), const String &path = String(), time_t expires = COOKIE_SESSION);

    HttpCookieHeader &setName(const String &name);
    HttpCookieHeader &setName(const __FlashStringHelper *name);
    HttpCookieHeader &setPath(const String &path);
    HttpCookieHeader &setPath(const __FlashStringHelper *path);
    HttpCookieHeader &setExpires(time_t expires);

#if HTTP_COOKIE_MAX_AGE_SUPPORT
    HttpCookieHeader &setMaxAge(int32_t maxAge);
#endif

    HttpCookieHeader &setHasExpired();

    HttpCookieHeader &setValue(const String &value);

    virtual bool equals(const HttpHeader &header) const override;
    bool equals(const HttpCookieHeader &header) const;

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

inline HttpCookieHeader::HttpCookieHeader(const String &name, const String &value, const String &path, time_t expires) :
    HttpHeader(FSPGM(Set_Cookie)),
    _cookieName(name),
    _value(value),
    _path(path)
{
    setExpires(expires);
}

inline HttpCookieHeader &HttpCookieHeader::setName(const String &name)
{
    _cookieName = name;
    return *this;
}

inline HttpCookieHeader &HttpCookieHeader::setName(const __FlashStringHelper *name)
{
    _cookieName = name;
    return *this;
}

inline HttpCookieHeader &HttpCookieHeader::setPath(const String &path)
{
    _path = path;
    return *this;
}

inline HttpCookieHeader &HttpCookieHeader::setPath(const __FlashStringHelper *path)
{
    _path = path;
    return *this;
}

inline HttpCookieHeader &HttpCookieHeader::setExpires(time_t expires)
{
    _expires = expires;
#if HTTP_COOKIE_MAX_AGE_SUPPORT
    if (expires == COOKIE_EXPIRED && _maxAge != MAX_AGE_NOT_SET) {
        _maxAge = 0;
    }
#endif
    return *this;
}

#if HTTP_COOKIE_MAX_AGE_SUPPORT
inline HttpCookieHeader &HttpCookieHeader::setMaxAge(int32_t maxAge)
{
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

inline HttpCookieHeader &HttpCookieHeader::setHasExpired()
{
    _expires = COOKIE_EXPIRED;
#if HTTP_COOKIE_MAX_AGE_SUPPORT
    _maxAge = MAX_AGE_NOT_SET;
#endif
    return *this;
}

inline HttpCookieHeader &HttpCookieHeader::setValue(const String &value)
{
    _value = value;
    return *this;
}

inline bool HttpCookieHeader::equals(const HttpHeader &header) const
{
    return equals(static_cast<const HttpCookieHeader&>(header));
}

inline bool HttpCookieHeader::equals(const HttpCookieHeader &header) const
{
    return (_name.equalsIgnoreCase(header._name) && _cookieName.equals(header._cookieName) && _path.equals(header._path)); // domain,secure,http only
}

// ------------------------------------------------------------------------
// HttpAuthorization
// ------------------------------------------------------------------------

class HttpAuthorization : public HttpSimpleHeader {
public:
    enum class AuthorizationType {
        BASIC = 0,
        BEARER = 1,
    } ;

    HttpAuthorization(const String &auth = String());
    HttpAuthorization(AuthorizationType type, const String &auth);
};

inline HttpAuthorization::HttpAuthorization(const String &auth) :
    HttpSimpleHeader(FSPGM(Authorization), auth)
{
}

inline HttpAuthorization::HttpAuthorization(AuthorizationType type, const String &auth) :
    HttpSimpleHeader(FSPGM(Authorization),
        type == AuthorizationType::BASIC ?
            F("Basic ") :
            FSPGM(Bearer_)
        )
{
    _header += auth;
}

// ------------------------------------------------------------------------
// HttpContentType
// ------------------------------------------------------------------------

class HttpContentType : public HttpSimpleHeader {
public:
    HttpContentType(const String &type);
    HttpContentType(const String &type, const String &charset);
};

inline HttpContentType::HttpContentType(const String &type) :
    HttpSimpleHeader(F("Content-Type"), type)
{
}

inline HttpContentType::HttpContentType(const String &type, const String &charset) :
    HttpSimpleHeader(F("Content-Type"), type)
{
    if (charset.length()) {
        _header.reserve(_header.length() + 10 + charset.length());
        _header += F("; charset=");
        _header += charset;
    }
}

class HttpHeaders {
public:
    using SetCallback = std::function<void(const String &name, const String &header)>;

    HttpHeaders();
    HttpHeaders(bool addDefault);
    HttpHeaders(HttpHeadersVector &&headers) noexcept : _headers(std::move(headers)) {}
    virtual ~HttpHeaders();

    HttpHeaders &operator=(HttpHeaders &&headers) noexcept;

    static String getRFC7231Date(const time_t *time);
    static HttpHeaders &getInstance();

    void init();
    void clear();
    void clear(uint8_t reserveItems);

    HttpHeader &add(HttpHeader *header);
    void add(const String& name, const String& value);
    HttpHeader &replace(HttpHeader *header);

    template <typename Th, typename ... Args>
    inline Th &add(Args &&...args) {
        return reinterpret_cast<Th &>(add(new Th(std::forward<Args &&>(args)...)));
    }

    template <typename Th, typename ... Args>
    inline Th &replace(Args &&...args) {
        return reinterpret_cast<Th &>(replace(new Th(std::forward<Args &&>(args)...)));
    }

    void remove(const String &name);
    void remove(const HttpHeader& header);

    HttpHeadersVector &getHeaders();
    void setHeaders(HttpHeadersVector &&headers);

    HttpHeadersIterator begin();
    HttpHeadersIterator end();

    HttpHeader *find(const String &name) const;
    HttpHeader *find(const __FlashStringHelper *name) const;
    HttpHeader *find(PGM_P name) const;

    void addNoCache(bool noStore = false);
    void addDefaultHeaders();
    void setHeadersCallback(SetCallback callback, bool doClear);

#if HAVE_HTTPHEADERS_ASYNCWEBSERVER

public:
    template <typename _Ta, typename std::enable_if<!std::is_base_of<AsyncBaseResponse, _Ta>::value, int>::type = 0>
    void setResponseHeaders(_Ta *response) {
        _setAsyncWebServerResponseHeaders(response);
    }

    template <typename _Ta, typename std::enable_if<std::is_base_of<AsyncBaseResponse, _Ta>::value, int>::type = 0>
    void setResponseHeaders(_Ta *response) {
        response->setHttpHeaders(std::move(_headers));
    }

private:
    void _setAsyncWebServerResponseHeaders(AsyncWebServerResponse *response);
#endif

public:
    void printTo(Print &output) const;
#if DEBUG
    void dump(Print &output) const;
#endif

private:
    HttpHeadersCmpFunction compareName(const String &name) const;
    HttpHeadersCmpFunction compareHeader(const HttpHeader &headerPtr) const;

    static HttpHeaders _instance;

    HttpHeadersVector _headers;
};

inline HttpHeaders::HttpHeaders()
{
    init();
}

inline HttpHeaders::HttpHeaders(bool addDefault)
{
    if (addDefault) {
        init();
    }
}


inline HttpHeaders::~HttpHeaders()
{
    _headers.clear();
}

inline HttpHeaders &HttpHeaders::operator=(HttpHeaders &&headers) noexcept
{
    _headers = std::move(headers._headers);
    return *this;
}

inline void HttpHeaders::clear()
{
    _headers.clear();
}

inline void HttpHeaders::clear(uint8_t reserveItems)
{
    _headers.clear();
    _headers.reserve(reserveItems);
}

inline void HttpHeaders::init()
{
    clear(5);
    addDefaultHeaders();
}

inline HttpHeader &HttpHeaders::add(HttpHeader *header)
{
    _headers.emplace_back(header);
    return *_headers.back().get();
}

inline void HttpHeaders::add(const String& name, const String& value)
{
    add(new HttpSimpleHeader(name, value));
}

inline HttpHeader &HttpHeaders::replace(HttpHeader *header)
{
    remove(*header);
    _headers.emplace_back(header);
    return *_headers.back().get();
}

inline void HttpHeaders::remove(const String &name)
{
    _headers.erase(std::remove_if(_headers.begin(), _headers.end(), compareName(name)), _headers.end());
}

inline void HttpHeaders::remove(const HttpHeader& header)
{
    _headers.erase(std::remove_if(_headers.begin(), _headers.end(), compareHeader(header)), _headers.end());
}

inline HttpHeadersVector &HttpHeaders::getHeaders()
{
    return _headers;
}

inline void HttpHeaders::setHeaders(HttpHeadersVector &&headers)
{
    _headers = std::move(headers);
}

inline HttpHeadersIterator HttpHeaders::begin()
{
    return _headers.begin();
}

inline HttpHeadersIterator HttpHeaders::end()
{
    return _headers.end();
}

inline HttpHeader *HttpHeaders::find(const String &name) const
{
    return find(name.c_str());
}

inline HttpHeader *HttpHeaders::find(const __FlashStringHelper *name) const
{
    return find(reinterpret_cast<PGM_P>(name));
}

inline HttpHeader *HttpHeaders::find(PGM_P name) const {
    auto iterator = std::find_if(_headers.begin(), _headers.end(), [name](const HttpHeaderPtr &header) {
        return strcasecmp_P(header->getName().c_str(), name) == 0;
    });
    if (iterator == _headers.end()) {
        return nullptr;
    }
    return (*iterator).get();
}
