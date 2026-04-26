# ESPAsyncHTTPClient
A simple lightweight HTTP client based on ESPAsyncTCP library with TLS support

## Summary
The purpose of this lib is to support  HTTP/HTTPS requests and process server responses asynchronously. The lib is based on the [ESPAsyncTCP fork](https://github.com/abratchik/ESPAsyncTCP), which serves as a transport layer interface similar to the WiFiClient.h.

## Key Features
- Fully asynchronous non-blocking operation
- Minimal memory requirements, which is critical for ESP8266
- Support for HTTPS secure connections
- API similar to ESP8266HTTPClient, simplifying migration

## Installation

### PlatformIO
Add the following to your `platformio.ini`:

```
library_dependencies =
	https://github.com/abratchik/ESPAsyncHTTPClient.git
```

Make sure you also install [ESPAsyncTCP](https://github.com/abratchik/ESPAsyncTCP) as a dependency.

### Arduino IDE
1. Download or clone this repository into your Arduino `libraries` folder.
2. Also install [ESPAsyncTCP](https://github.com/abratchik/ESPAsyncTCP) in the same way.

## Configuration

### Required Libraries
- ESPAsyncTCP (custom fork: https://github.com/abratchik/ESPAsyncTCP)
- ESP8266WiFi or ESP32 WiFi library

### PlatformIO Build Flags
To enable SSL/TLS support, add the following flags to your `platformio.ini`:

```
-D ASYNC_TCP_SSL_ENABLED=1
-D ASYNC_TCP_SSL_IN_BUFFER_SIZE=2048
-D ASYNC_TCP_SSL_OUT_BUFFER_SIZE=2048
-D ASYNC_TCP_SSL_X509_MODE=1
```

## Usage

### Basic Example

```cpp
#include <ESP8266WiFi.h>
#include "AsyncHTTPClient.h"

AsyncHTTPClient httpClient;

void setup() {
	Serial.begin(115200);
	WiFi.begin("SSID", "PASSWORD");
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
	}
	if (httpClient.begin("http://httpbin.org/get")) {
		httpClient.GET("",
			[](int statusCode, const String& body) {
				Serial.printf("Response status: %d\n", statusCode);
				Serial.println(body);
			},
			[](int errorCode) {
				Serial.printf("Error: %d\n", errorCode);
			}
		);
	}
}

void loop() {}
```

## HTTPS Support

To access HTTPS sites, ensure SSL is enabled (see Configuration above). Then simply use an `https://` URL in `begin()`:

```cpp
httpClient.begin("https://example.com/api");
```

No additional certificate handling is required for most use cases. For advanced certificate validation, refer to ESPAsyncTCP documentation.

## Working with HTTP Headers

You can add custom headers to your requests using `addHeader()`:

```cpp
httpClient.addHeader("Authorization", "Bearer <token>");
httpClient.addHeader("Custom-Header", "Value");
```

To collect and access response headers:

```cpp
const char* keys[] = {"Content-Type", "Set-Cookie"};
httpClient.collectHeaders(keys, 2);
// After response:
String contentType = httpClient.header("Content-Type");
```

## Examples

See the [examples/SimpleClient/SimpleClient.ino](examples/SimpleClient/SimpleClient.ino) for a complete example including WiFi connection and HTTPS usage.


