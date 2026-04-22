#include "AsyncHTTPClient.h"

AsyncHTTPClient::AsyncHTTPClient()
    : _client(nullptr)
    , _use_tls(false)
    , _state(STATE_IDLE)
    , _port(80)
    , _timeout(5)
    , _followRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS)
    , _redirectLimit(10)
    , _reuse(true)
    , _useHTTP10(false)
    , _statusCode(0)
    , _contentLength(0)
    , _transferEncoding(HTTPC_TE_IDENTITY)    , _chunkSize(0){
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
    if(https && !ASYNC_TCP_SSL_ENABLED) {
        DEBUG_ASYNC_HTTP("HTTPS not supported in this build\n");
        return false;
     }
    _use_tls = https;
    if (_use_tls && _port == 80) _port = 443;
    return true;
}

void AsyncHTTPClient::end() {
    _releaseClient();
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
    if(protocol == "https" && ASYNC_TCP_SSL_ENABLED) {
        _use_tls = true;
    } else if(protocol == "http") {
        _use_tls = false;
    } else {
        DEBUG_ASYNC_HTTP("Unsupported protocol: %s\n", protocol.c_str());
        return false; 
    }
    
    int hostStart = protocolEnd + 3;
    int pathStart = url.indexOf('/', hostStart);
    if (pathStart == -1) {
        _host = url.substring(hostStart);
        _uri = default_uri;
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

void AsyncHTTPClient::GET(const String& uri,  OnResponseCallback onComplete, OnErrorCallback onError) {
    _sendRequest("GET", "", 0, _uri, onComplete, onError);
}

void AsyncHTTPClient::POST(const String& payload, const String& uri,  OnResponseCallback onComplete, OnErrorCallback onError) {
    _sendRequest("POST", payload, payload.length(), uri, onComplete, onError);
}

void AsyncHTTPClient::POST(const uint8_t* payload, size_t size, const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    sendRequest("POST", payload, size, uri, onComplete, onError);
}

void AsyncHTTPClient::PUT(const String& payload, const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    _sendRequest("PUT", payload, payload.length(), uri, onComplete, onError);
}

void AsyncHTTPClient::PUT(const uint8_t* payload, size_t size, const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    sendRequest("PUT", payload, size, uri, onComplete, onError);
}

void AsyncHTTPClient::PATCH(const String& payload, const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    _sendRequest("PATCH", payload, payload.length(), uri, onComplete, onError);
}

void AsyncHTTPClient::PATCH(const uint8_t* payload, size_t size, const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    sendRequest("PATCH", payload, size, uri, onComplete, onError);
}

void AsyncHTTPClient::DELETE(const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    _sendRequest("DELETE", "", 0, uri, onComplete, onError);
}

void AsyncHTTPClient::sendRequest(const char* type, const String& payload, const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    _sendRequest(type, payload, payload.length(), uri, onComplete, onError);
}

void AsyncHTTPClient::sendRequest(const char* type, const uint8_t* payload, size_t size, const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    _payload = String();
    _payload.concat((const char*)payload, size);
    _sendRequest(type, _payload, size, uri, onComplete, onError);
}

void AsyncHTTPClient::_sendRequest(const char* type, const String& payload, size_t size, const String& uri, OnResponseCallback onComplete, OnErrorCallback onError) {
    if (_state != STATE_IDLE) {
        _failRequest(ERR_ALREADY);
        return;
    }

    _method = type;
    _payload = payload;
    _payloadSize = size;
    _uri = uri.isEmpty() ? _uri : uri;
    _onComplete = onComplete;
    _onError = onError;

    // Reset response data
    _statusCode = 0;
    _responseHeaders = "";
    _responseBody = "";
    _contentLength = 0;
    _transferEncoding = HTTPC_TE_IDENTITY;
    _location = "";
    _chunkSize = 0;
    _chunkBuffer = "";
    
    if (!_client) {
        _client = new AsyncClient();
        // Set callbacks
        _client->onConnect(_onConnectCB, this);
        _client->onDisconnect(_onDisconnectCB, this);
        _client->onData(_onDataCB, this);
        _client->onError(_onErrorCB, this);

        _client->setRxTimeout(_timeout);
    }

    DEBUG_ASYNC_HTTP("Connecting to %s:%d\n", _host.c_str(), _port);

    if(!_client->connected()) {
        // Connect
        _transitionState(STATE_CONNECTING);
    #if ASYNC_TCP_SSL_ENABLED
        if (!_client->connect(_host.c_str(), _port, _use_tls)) 
    #else
        if (!_client->connect(_host.c_str(), _port)) 
    #endif
        {
            _failRequest(ERR_CONN);
        }
    }
    else {
        _handleConnect(); // Already connected (e.g. connection reuse) - skip straight to sending request
    }
}

void AsyncHTTPClient::_transitionState(State newState) {
    DEBUG_ASYNC_HTTP("State transition: %d -> %d\n", _state, newState);
    _state = newState;
}

void AsyncHTTPClient::_completeRequest() {
    _transitionState(STATE_COMPLETE);
    
    // Call callback BEFORE any cleanup
    if (_onComplete) {
        _onComplete(_statusCode, _responseBody);
    }
    
    // Transition back to IDLE
    _transitionState(STATE_IDLE);
    
}

void AsyncHTTPClient::_failRequest(int errorCode) {
    _transitionState(STATE_ERROR);
    if (_onError) {
        _onError(errorCode);
    }
    
    // Do NOT close from callback context - let it close naturally
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
    // Handle disconnection - complete request if we were waiting for data
    DEBUG_ASYNC_HTTP("Disconnected from %s, state=%d\n", _host.c_str(), _state);

    if (_state == STATE_RECEIVING_BODY || _state == STATE_RECEIVING_HEADERS) {
        _transitionState(STATE_COMPLETE);
        if (_onComplete) {
            _onComplete(_statusCode, _responseBody);
        }
        _transitionState(STATE_IDLE);
    } else if (_state != STATE_IDLE) {
        // If we're in an error state or other state, just go idle
        _transitionState(STATE_IDLE);
    }
}

void AsyncHTTPClient::_handleData(void* data, size_t len) {
    if (len == 0) {
        return;
    }
    
    if (_state == STATE_RECEIVING_HEADERS) {
        String newData;
        newData.concat((char*)data, len);
        _responseHeaders += newData;
        
        bool headerComplete = false;
        if (_responseHeaders.indexOf("\r\n\r\n") != -1) {
            headerComplete = true;
        } else if (_responseHeaders.length() >= 4 && 
                   _responseHeaders.endsWith("\r\n\r") && 
                   newData.startsWith("\n")) {
            // Handle case where "\r\n\r\n" is split across packets
            headerComplete = true;
        }
        
        if (headerComplete) {
            _parseHeaders();
            // If there's body data in the same packet, append it
            if (_responseHeaders.length() > 0) {
                _responseBody += _responseHeaders;
                _responseHeaders = "";
            }
            if (_contentLength == 0) {
                _completeRequest();
            } else {
                _transitionState(STATE_RECEIVING_BODY);
            }
        }
    } else if (_state == STATE_RECEIVING_BODY) {
        if (_transferEncoding == HTTPC_TE_IDENTITY) {
            _responseBody.concat((char*)data, len);
            if (_contentLength > 0 && _responseBody.length() >= _contentLength) {
                _completeRequest();
            }
        } else if (_transferEncoding == HTTPC_TE_CHUNKED) {
            _chunkBuffer.concat((char*)data, len);
            _parseChunks();
        }
    }
}

void AsyncHTTPClient::_handleError(int error) {
    // if (_state == STATE_IDLE) return;  // Ignore errors after request completion
    _failRequest(error);
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

void AsyncHTTPClient::_parseChunks() {
    while (_chunkBuffer.length() > 0) {
        if (_chunkSize == 0) {
            // Looking for chunk size
            int rn = _chunkBuffer.indexOf("\r\n");
            if (rn == -1) {
                // Need more data
                break;
            }
            String sizeLine = _chunkBuffer.substring(0, rn);
            _chunkBuffer = _chunkBuffer.substring(rn + 2);
            
            // Parse hex size
            char* endptr;
            _chunkSize = strtol(sizeLine.c_str(), &endptr, 16);
            
            if (_chunkSize == 0) {
                // Last chunk, check for trailing \r\n\r\n
                if (_chunkBuffer.startsWith("\r\n")) {
                    _chunkBuffer = _chunkBuffer.substring(2);
                }
                _completeRequest();
                break;
            }
        } else {
            // Reading chunk data
            if (_chunkBuffer.length() >= _chunkSize + 2) { // +2 for \r\n
                String chunkData = _chunkBuffer.substring(0, _chunkSize);
                _responseBody += chunkData;
                _chunkBuffer = _chunkBuffer.substring(_chunkSize + 2); // Skip \r\n
                _chunkSize = 0; // Next chunk size
            } else {
                // Need more data for this chunk
                break;
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

const String& AsyncHTTPClient::getUri() const {
    return _uri;
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
    if(_client) {
        _client->abort();
    }
    _transitionState(STATE_IDLE);
}

void AsyncHTTPClient::_releaseClient(bool immediately) {
    if (_client) {
        _client->close(immediately);
        delete _client;
        _client = nullptr;
    }
}