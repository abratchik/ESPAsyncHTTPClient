#include <ESP8266WiFi.h>
#include "AsyncHTTPClient.h"

#include "config.h"

AsyncHTTPClient httpClient;

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.printf("Connected to WiFi, local IP: %s\n", WiFi.localIP().toString().c_str());

#if ASYNC_TCP_SSL_ENABLED
    if (httpClient.begin("https://httpbin.org/get")) {
#else
    if (httpClient.begin("http://httpbin.org/get")) {
#endif
        Serial.println("Making GET request...");
        httpClient.GET(
            [](int statusCode, const String& body) {
                Serial.printf("Response status: %d\n", statusCode);
                Serial.println("Response body:");
                Serial.println(body);
            },
            [](const String& error) {
                Serial.println("Error: " + error);
            }
        );
    } else {
        Serial.println("Failed to init HTTP client");
    }
}

void loop() {
    // Nothing to do here, request is async
}