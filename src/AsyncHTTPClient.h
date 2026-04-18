#ifndef ASYNCHTTPCLIENT_H
#define ASYNCHTTPCLIENT_H   

#include <ESPAsyncTCP.h>
#include <functional>
#include <vector>

#if DEBUG_ESP_ASYNC_HTTP_CLIENT && (DEBUG_ESP_PORT == Serial)
#define DEBUG_ASYNC_HTTP(format, ...) DEBUG_GENERIC_F("[ASYNC_HTTP]", format, ##__VA_ARGS__)
#endif

#ifndef DEBUG_ASYNC_HTTP
#define DEBUG_ASYNC_HTTP(...) do { (void)0; } while (0)
#endif

/// HTTP codes see RFC7231
typedef enum {
    HTTP_CODE_CONTINUE = 100,
    HTTP_CODE_SWITCHING_PROTOCOLS = 101,
    HTTP_CODE_PROCESSING = 102,
    HTTP_CODE_OK = 200,
    HTTP_CODE_CREATED = 201,
    HTTP_CODE_ACCEPTED = 202,
    HTTP_CODE_NON_AUTHORITATIVE_INFORMATION = 203,
    HTTP_CODE_NO_CONTENT = 204,
    HTTP_CODE_RESET_CONTENT = 205,
    HTTP_CODE_PARTIAL_CONTENT = 206,
    HTTP_CODE_MULTI_STATUS = 207,
    HTTP_CODE_ALREADY_REPORTED = 208,
    HTTP_CODE_IM_USED = 226,
    HTTP_CODE_MULTIPLE_CHOICES = 300,
    HTTP_CODE_MOVED_PERMANENTLY = 301,
    HTTP_CODE_FOUND = 302,
    HTTP_CODE_SEE_OTHER = 303,
    HTTP_CODE_NOT_MODIFIED = 304,
    HTTP_CODE_USE_PROXY = 305,
    HTTP_CODE_TEMPORARY_REDIRECT = 307,
    HTTP_CODE_PERMANENT_REDIRECT = 308,
    HTTP_CODE_BAD_REQUEST = 400,
    HTTP_CODE_UNAUTHORIZED = 401,
    HTTP_CODE_PAYMENT_REQUIRED = 402,
    HTTP_CODE_FORBIDDEN = 403,
    HTTP_CODE_NOT_FOUND = 404,
    HTTP_CODE_METHOD_NOT_ALLOWED = 405,
    HTTP_CODE_NOT_ACCEPTABLE = 406,
    HTTP_CODE_PROXY_AUTHENTICATION_REQUIRED = 407,
    HTTP_CODE_REQUEST_TIMEOUT = 408,
    HTTP_CODE_CONFLICT = 409,
    HTTP_CODE_GONE = 410,
    HTTP_CODE_LENGTH_REQUIRED = 411,
    HTTP_CODE_PRECONDITION_FAILED = 412,
    HTTP_CODE_PAYLOAD_TOO_LARGE = 413,
    HTTP_CODE_URI_TOO_LONG = 414,
    HTTP_CODE_UNSUPPORTED_MEDIA_TYPE = 415,
    HTTP_CODE_RANGE_NOT_SATISFIABLE = 416,
    HTTP_CODE_EXPECTATION_FAILED = 417,
    HTTP_CODE_MISDIRECTED_REQUEST = 421,
    HTTP_CODE_UNPROCESSABLE_ENTITY = 422,
    HTTP_CODE_LOCKED = 423,
    HTTP_CODE_FAILED_DEPENDENCY = 424,
    HTTP_CODE_UPGRADE_REQUIRED = 426,
    HTTP_CODE_PRECONDITION_REQUIRED = 428,
    HTTP_CODE_TOO_MANY_REQUESTS = 429,
    HTTP_CODE_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
    HTTP_CODE_NOT_IMPLEMENTED = 501,
    HTTP_CODE_BAD_GATEWAY = 502,
    HTTP_CODE_SERVICE_UNAVAILABLE = 503,
    HTTP_CODE_GATEWAY_TIMEOUT = 504,
    HTTP_CODE_HTTP_VERSION_NOT_SUPPORTED = 505,
    HTTP_CODE_VARIANT_ALSO_NEGOTIATES = 506,
    HTTP_CODE_INSUFFICIENT_STORAGE = 507,
    HTTP_CODE_LOOP_DETECTED = 508,
    HTTP_CODE_NOT_EXTENDED = 510,
    HTTP_CODE_NETWORK_AUTHENTICATION_REQUIRED = 511
} t_http_codes;

typedef enum {
    HTTPC_TE_IDENTITY,
    HTTPC_TE_CHUNKED
} transferEncoding_t;

/**
 * redirection follow mode.
 * + `HTTPC_DISABLE_FOLLOW_REDIRECTS` - no redirection will be followed.
 * + `HTTPC_STRICT_FOLLOW_REDIRECTS` - strict RFC2616, only requests using
 *      GET or HEAD methods will be redirected (using the same method),
 *      since the RFC requires end-user confirmation in other cases.
 * + `HTTPC_FORCE_FOLLOW_REDIRECTS` - all redirections will be followed,
 *      regardless of a used method. New request will use the same method,
 *      and they will include the same body data and the same headers.
 *      In the sense of the RFC, it's just like every redirection is confirmed.
 */
typedef enum {
    HTTPC_DISABLE_FOLLOW_REDIRECTS,
    HTTPC_STRICT_FOLLOW_REDIRECTS,
    HTTPC_FORCE_FOLLOW_REDIRECTS
} followRedirects_t;

// Callback types for async responses
using OnResponseCallback = std::function<void(int statusCode, const String& body)>;
using OnErrorCallback = std::function<void(const String& error)>;

class AsyncHTTPClient {
public:
    AsyncHTTPClient();
    ~AsyncHTTPClient();
    AsyncHTTPClient(AsyncHTTPClient&&) = delete;
    AsyncHTTPClient& operator=(AsyncHTTPClient&&) = delete;

    // Initialization
    bool begin(const String& url);
    bool begin(const String& host, uint16_t port, const String& uri, bool https = false);
    void end();

    // HTTP Methods
    void GET(OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void POST(const String& payload, OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void POST(const uint8_t* payload, size_t size, OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void PUT(const String& payload, OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void PUT(const uint8_t* payload, size_t size, OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void PATCH(const String& payload, OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void PATCH(const uint8_t* payload, size_t size, OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void DELETE(OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void sendRequest(const char* type, const String& payload = "", OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);
    void sendRequest(const char* type, const uint8_t* payload, size_t size, OnResponseCallback onComplete = nullptr, OnErrorCallback onError = nullptr);

    // Header Management
    void addHeader(const String& name, const String& value, bool first = false, bool replace = true);
    void collectHeaders(const char* headerKeys[], size_t count);
    const String& header(const String& name);
    const String& header(size_t i);
    const String& headerName(size_t i);
    size_t headers();
    bool hasHeader(const String& name);
    void clearHeaders();

    // Configuration
    void setTimeout(uint32_t timeout);
    void setFollowRedirects(followRedirects_t follow);
    void setRedirectLimit(uint16_t limit);
    void setUserAgent(const String& userAgent);
    void setAuthorization(const char* user, const char* password);
    void setAuthorization(const String& auth);
    void setReuse(bool reuse);
    void useHTTP10(bool usehttp10);

    // Status
    bool connected() const;
    int getStatusCode() const;
    const String& getString() const;
    size_t getSize() const;
    const String& getLocation() const;
    void abort();

private:
    enum State {
        STATE_IDLE,
        STATE_CONNECTING,
        STATE_SENDING_REQUEST,
        STATE_SENDING_BODY,
        STATE_RECEIVING_HEADERS,
        STATE_RECEIVING_BODY,
        STATE_COMPLETE,
        STATE_ERROR
    };

    AsyncClient* _client;
    bool _use_tls;
    State _state;
    String _host;
    uint16_t _port;
    String _uri;
    String _method;
    String _request;
    String _payload;
    size_t _payloadSize;
    uint32_t _timeout;
    followRedirects_t _followRedirects;
    uint16_t _redirectLimit;
    String _userAgent;
    String _authorization;
    bool _reuse;
    bool _useHTTP10;

    // Headers
    struct Header {
        String name;
        String value;
    };
    std::vector<Header> _headers;
    std::vector<String> _collectHeaders;

    // Response
    int _statusCode;
    String _responseHeaders;
    String _responseBody;
    size_t _contentLength;
    transferEncoding_t _transferEncoding;
    String _location;

    // Callbacks
    OnResponseCallback _onComplete;
    OnErrorCallback _onError;

    // Internal methods
    bool _parseURL(const String& url);
    void _buildRequest();
    void _sendRequest();
    void _handleConnect();
    void _handleDisconnect();
    void _handleData(void* data, size_t len);
    void _handleError(int error);
    void _parseHeaders();
    void _transitionState(State newState);
    void _completeRequest();
    void _failRequest(const String& error);
    static void _onConnectCB(void* arg, AsyncClient* client);
    static void _onDisconnectCB(void* arg, AsyncClient* client);
    static void _onDataCB(void* arg, AsyncClient* client, void* data, size_t len);
    static void _onErrorCB(void* arg, AsyncClient* client, int error);
};

#endif /* ASYNCHTTPCLIENT_H */