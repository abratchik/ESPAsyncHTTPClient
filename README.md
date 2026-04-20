# ESPAsyncHTTPClient
A simple lightweight HTTP client based on ESPAsyncTCP library with TLS support

## Summary
The purpose of this lib is to support  HTTP/HTTPS requests and process server responses asynchronously. The lib is based on the [ESPAsyncTCP fork](https://github.com/abratchik/ESPAsyncTCP), which serves as a transport layer similar to the WiFiClient.h.

## Key features
 - Fully asynchronos non-bloking operation
 - Minimal memory requirements, which is critical for ESP8266. 
 - Support of HTTPS secure connections.  
 - The API is similar to ESP8266HTTPClient thus simplifying migration.
