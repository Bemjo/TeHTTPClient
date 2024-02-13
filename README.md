# Description
An Arduino Client wrapper written in C++ that implements an HTTPClient to read HTTP data streams.  
Implements a subset of HTTP/1.1, specifically reading chunked data connections.  

## Installation & Usage
This is a header and source file library, place them where you need and update the source file import to point at the header file if you have placed it seperatly from the source file.  

### Example
```
#include <SPI.h>
#include <WiFi.h>
#include "HTTPClient.h"

const int BUFFER_SIZE = 1024;

char ssid[] = "myNetwork";          //  your network SSID (name)
char pass[] = "myPassword";   // your network password

int status = WL_IDLE_STATUS;
char server[] = "74.125.115.105";  // Google
const char request[] = "/search?q=arduino";

// Initialize the client library
WiFiClient wifiClient;
HTTPClient httpClient(wifiClient);

// Called everytime the read buffer is filled from requests
bool writeCallback(uint8_t *buffer, size_t dataSize) {
  Serial.println("Receieved %d bytes", dataSize);

  for (int i = 0; i < dataSize; ++i) {
    Serial.print("%02X , buffer[i]);
  }
  Serial.print("\n");

  if (dataSize < BUFFER_SIZE) {
    Serial.println("End of File");
  }
}

void setup() {
  // We initialize our WiFi client as usual
  status = WiFi.begin(ssid, pass);
  Serial.begin(9600);
  delay(1000);
  
  if (status != WL_CONNECTED) {
    Serial.println("Couldn't get a wifi connection");
    while(true);
  } else {
    Serial.println("Connected to wifi");
    Serial.println("\nStarting connection...");

    
    std::shared_ptr<ConnectionInformation> res = httpClient.http_get(server, 80, request);

    if (res == nullptr || res->return_status != 200) {
      Serial.println("Failed to make get request to %s%s", server, request);
    } else {
      uint8_t data[BUFFER_SIZE];
      long int suc = readBody(data, IMAGE_BUFFER_SIZE, writeCallback);

      if (suc <= 0) {
        Serial.println("Failed to read data");
      }
    }
  }
}
```

## Hardware Requirements
An Arduino compatible board.  
Arduino compatible hardware (Ethernet, Wifi) to initialize a Client connection.  

### Libraries:
The C++ STD Library  
[StreamUtils](https://www.arduino.cc/reference/en/libraries/streamutils/)  
[ArduinoJson](https://www.arduino.cc/reference/en/libraries/arduinojson/)  