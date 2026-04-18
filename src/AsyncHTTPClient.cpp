#include "AsyncHTTPClient.h"

AsyncHTTPClient::AsyncHTTPClient()
    : _client(nullptr)
    , _use_tls(false)
    , _state(STATE_IDLE)
    , _port(80)
    , _timeout(5000)
    , _followRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS)
    , _redirectLimit(10)
    , _reuse(true)
    , _useHTTP10(false)
    , _statusCode(0)
    , _contentLength(0)
    , _transferEncoding(HTTPC_TE_IDENTITY)
{
    _userAgent = "AsyncHTTPClient";
}

AsyncHTTPClient::~AsyncHTTPClient() {
    end();
}

bool AsyncHTTPClient::begin(const String& url) {
    if (_parseURL(url)) {
        return true;
    }
    return false;
}

bool AsyncHTTPClient::begin(const String& host, uint16_t port, const String& uri, bool https) {
    _host = host;
    _port = port;
    _uri = uri;
    _use_tls = https;
    if (_use_tls && _port == 80) _port = 443;
    return true;
}

void AsyncHTTPClient::end() {
    if (_client) {
        _client->close();
        delete _client;
        _client = nullptr;
    }
    _state = STATE_IDLE;
    _headers.clear();
    _collectHeaders.clear();
    _responseHeaders = "";
    _responseBody = "";
    _statusCode = 0;
    _contentLength = 0;
}

bool AsyncHTTPClient::_parseURL(const String& url) {
    // Simple URL parser: http://host:port/path or https://host:port/path
    int protocolEnd = url.indexOf("://");
    if (protocolEnd == -1) return false;
    
    String protocol = url.substring(0, protocolEnd);
    _use_tls = (protocol == "https");
    
    int hostStart = protocolEnd + 3;
    int pathStart = url.indexOf('/', hostStart);
    if (pathStart == -1) {
        _host = url.substring(hostStart);
        _uri = "/";
    } else {
        _host = url.substring(hostStart, pathStart);
        _uri = url.substring(pathStart);
    }
    
    int portStart = _host.indexOf(':');
    if (portStart != -1) {
        _port = _host.substring(portStart + 1).toInt();
        _host = _host.substring(0, portStart);
    } else {
        _port = _use_tls ? 443 : 80;
    }

    DEBUG_ASYNC_HTTP("Parsed URL: protocol=%s, host=%s, port=%d, uri=%s\n", protocol.c_str(), _host.c_str(), _port, _uri.c_str());
    
    return true;
}

void AsyncHTTPClient::GET(OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = "GET";
    _payload = "";
    _payloadSize = 0;
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::POST(const String& payload, OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = "POST";
    _payload = payload;
    _payloadSize = payload.length();
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::POST(const uint8_t* payload, size_t size, OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = "POST";
    _payload = String((const char*)payload);
    _payloadSize = size;
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::PUT(const String& payload, OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = "PUT";
    _payload = payload;
    _payloadSize = payload.length();
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::PUT(const uint8_t* payload, size_t size, OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = "PUT";
    _payload = String((const char*)payload);
    _payloadSize = size;
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::PATCH(const String& payload, OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = "PATCH";
    _payload = payload;
    _payloadSize = payload.length();
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::PATCH(const uint8_t* payload, size_t size, OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = "PATCH";
    _payload = String((const char*)payload);
    _payloadSize = size;
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::DELETE(OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = "DELETE";
    _payload = "";
    _payloadSize = 0;
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::sendRequest(const char* type, const String& payload, OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = type;
    _payload = payload;
    _payloadSize = payload.length();
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::sendRequest(const char* type, const uint8_t* payload, size_t size, OnResponseCallback onComplete, OnErrorCallback onError) {
    _method = type;
    _payload = String((const char*)payload);
    _payloadSize = size;
    _onComplete = onComplete;
    _onError = onError;
    _sendRequest();
}

void AsyncHTTPClient::_sendRequest() {
    if (_state != STATE_IDLE) {
        _failRequest("Request already in progress");
        return;
    }
    
    // Reset response data
    _statusCode = 0;
    _responseHeaders = "";
    _responseBody = "";
    _contentLength = 0;
    _transferEncoding = HTTPC_TE_IDENTITY;
    _location = "";
    
    // Create client
    if (!_client) {
        _client = new AsyncClient();
        // Set callbacks
        _client->onConnect(_onConnectCB, this);
        _client->onDisconnect(_onDisconnectCB, this);
        _client->onData(_onDataCB, this);
        _client->onError(_onErrorCB, this);
    }

    // Connect
    _transitionState(STATE_CONNECTING);

#if ASYNC_TCP_SSL_ENABLED
    if (!_client->connect(_host.c_str(), _port, _use_tls)) {
#else
    if (!_client->connect(_host.c_str(), _port)) {
#endif
        _failRequest("Connection failed");
    }
}

void AsyncHTTPClient::_transitionState(State newState) {
    DEBUG_ASYNC_HTTP("State transition: %d -> %d\n", _state, newState);
    _state = newState;
}

void AsyncHTTPClient::_completeRequest() {
    _transitionState(STATE_COMPLETE);
    if (_onComplete) {
        _onComplete(_statusCode, _responseBody);
    }
    _transitionState(STATE_IDLE);
}

void AsyncHTTPClient::_failRequest(const String& error) {
    _transitionState(STATE_ERROR);
    if (_onError) {
        _onError(error);
    }
    _transitionState(STATE_IDLE);
}

// Static callback handlers
void AsyncHTTPClient::_onConnectCB(void* arg, AsyncClient* client) {
    AsyncHTTPClient* self = (AsyncHTTPClient*)arg;
    self->_handleConnect();
}

void AsyncHTTPClient::_onDisconnectCB(void* arg, AsyncClient* client) {
    AsyncHTTPClient* self = (AsyncHTTPClient*)arg;
    self->_handleDisconnect();
}

void AsyncHTTPClient::_onDataCB(void* arg, AsyncClient* client, void* data, size_t len) {
    AsyncHTTPClient* self = (AsyncHTTPClient*)arg;
    self->_handleData(data, len);
}

void AsyncHTTPClient::_onErrorCB(void* arg, AsyncClient* client, int error) {
    AsyncHTTPClient* self = (AsyncHTTPClient*)arg;
    self->_handleError(error);
}

void AsyncHTTPClient::_handleConnect() {
    DEBUG_ASYNC_HTTP("Connected to %s, state=%d\n", _host.c_str(), _state);

    if (_state != STATE_CONNECTING) return;
    
    _transitionState(STATE_SENDING_REQUEST);
    _buildRequest();
    _client->write(_request.c_str(), _request.length());
    
    if (_payloadSize > 0) {
        _transitionState(STATE_SENDING_BODY);
        _client->write(_payload.c_str(), _payloadSize);
    }
    
    _transitionState(STATE_RECEIVING_HEADERS);
}

void AsyncHTTPClient::_handleDisconnect() {
    // Handle disconnection
    if (_state == STATE_RECEIVING_BODY || _state == STATE_RECEIVING_HEADERS) {
        _completeRequest();
    }
}

void AsyncHTTPClient::_handleData(void* data, size_t len) {
    char* buf = (char*)data;
    
    if (_state == STATE_RECEIVING_HEADERS) {
        _responseHeaders += String(buf);
        if (_responseHeaders.indexOf("\r\n\r\n") != -1) {
            _parseHeaders();
            _transitionState(STATE_RECEIVING_BODY);
        }
    } else if (_state == STATE_RECEIVING_BODY) {
        _responseBody += String(buf);
        if (_contentLength > 0 && _responseBody.length() >= _contentLength) {
            _completeRequest();
        }
    }
}

void AsyncHTTPClient::_handleError(int error) {
    _failRequest("Connection error: " + String(error));
}

void AsyncHTTPClient::_buildRequest() {
    _request = _method + " " + _uri + " HTTP/1.1\r\n";
    _request += "Host: " + _host + "\r\n";
    _request += "User-Agent: " + _userAgent + "\r\n";
    if (_authorization.length() > 0) {
        _request += "Authorization: " + _authorization + "\r\n";
    }
    if (_payloadSize > 0) {
        _request += "Content-Length: " + String(_payloadSize) + "\r\n";
    }
    for (auto& header : _headers) {
        _request += header.name + ": " + header.value + "\r\n";
    }
    _request += "\r\n";
}

void AsyncHTTPClient::_parseHeaders() {
    int headerEnd = _responseHeaders.indexOf("\r\n\r\n");
    if (headerEnd == -1) return;
    
    String headers = _responseHeaders.substring(0, headerEnd);
    _responseHeaders = _responseHeaders.substring(headerEnd + 4);
    
    // Parse status line
    int firstLineEnd = headers.indexOf("\r\n");
    String statusLine = headers.substring(0, firstLineEnd);
    int space1 = statusLine.indexOf(' ');
    int space2 = statusLine.indexOf(' ', space1 + 1);
    _statusCode = statusLine.substring(space1 + 1, space2).toInt();
    
    // Parse headers
    String remaining = headers.substring(firstLineEnd + 2);
    while (remaining.length() > 0) {
        int lineEnd = remaining.indexOf("\r\n");
        if (lineEnd == -1) break;
        String line = remaining.substring(0, lineEnd);
        remaining = remaining.substring(lineEnd + 2);
        
        int colon = line.indexOf(':');
        if (colon != -1) {
            String name = line.substring(0, colon);
            String value = line.substring(colon + 1);
            value.trim();
            if (name.equalsIgnoreCase("Content-Length")) {
                _contentLength = value.toInt();
            } else if (name.equalsIgnoreCase("Location")) {
                _location = value;
            } else if (name.equalsIgnoreCase("Transfer-Encoding")) {
                if (value.equalsIgnoreCase("chunked")) {
                    _transferEncoding = HTTPC_TE_CHUNKED;
                }
            }
        }
    }
}

void AsyncHTTPClient::addHeader(const String& name, const String& value, bool first, bool replace) {
    // Simplified: just add to vector
    _headers.push_back({name, value});
}

void AsyncHTTPClient::collectHeaders(const char* headerKeys[], size_t count) {
    for (size_t i = 0; i < count; i++) {
        _collectHeaders.push_back(headerKeys[i]);
    }
}

const String& AsyncHTTPClient::header(const String& name) {
    for (auto& h : _headers) {
        if (h.name.equalsIgnoreCase(name)) {
            return h.value;
        }
    }
    static String empty = "";
    return empty;
}

const String& AsyncHTTPClient::header(size_t i) {
    if (i < _headers.size()) {
        return _headers[i].value;
    }
    static String empty = "";
    return empty;
}

const String& AsyncHTTPClient::headerName(size_t i) {
    if (i < _headers.size()) {
        return _headers[i].name;
    }
    static String empty = "";
    return empty;
}

size_t AsyncHTTPClient::headers() {
    return _headers.size();
}

bool AsyncHTTPClient::hasHeader(const String& name) {
    for (auto& h : _headers) {
        if (h.name.equalsIgnoreCase(name)) {
            return true;
        }
    }
    return false;
}

void AsyncHTTPClient::clearHeaders() {
    _headers.clear();
}

void AsyncHTTPClient::setTimeout(uint32_t timeout) {
    _timeout = timeout;
    if (_client) {
        _client->setRxTimeout(_timeout);
    }
}

void AsyncHTTPClient::setFollowRedirects(followRedirects_t follow) {
    _followRedirects = follow;
}

void AsyncHTTPClient::setRedirectLimit(uint16_t limit) {
    _redirectLimit = limit;
}

void AsyncHTTPClient::setUserAgent(const String& userAgent) {
    _userAgent = userAgent;
}

void AsyncHTTPClient::setAuthorization(const char* user, const char* password) {
    String auth = "Basic ";
    // Need base64 encoding, but for now placeholder
    auth += String(user) + ":" + String(password); // TODO: base64 encode
    _authorization = auth;
}

void AsyncHTTPClient::setAuthorization(const String& auth) {
    _authorization = auth;
}

void AsyncHTTPClient::setReuse(bool reuse) {
    _reuse = reuse;
}

void AsyncHTTPClient::useHTTP10(bool usehttp10) {
    _useHTTP10 = usehttp10;
}

bool AsyncHTTPClient::connected() const {
    return _client && _client->connected();
}

int AsyncHTTPClient::getStatusCode() const {
    return _statusCode;
}

const String& AsyncHTTPClient::getString() const {
    return _responseBody;
}

size_t AsyncHTTPClient::getSize() const {
    return _contentLength;
}

const String& AsyncHTTPClient::getLocation() const {
    return _location;
}

void AsyncHTTPClient::abort() {
    if (_client) {
        _client->close();
    }
    _transitionState(STATE_IDLE);
}