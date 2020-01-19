/**
  A simple stream handler to play web radio stations using ESP8266

  Copyright (C) 2018 Vince Gellár (github.com/vincegellar)
  Licensed under GNU GPL v3
  
  Wiring:
  --------------------------------
  | VS1053  | ESP8266 |  ESP32   |
  --------------------------------
  |   SCK   |   D5    |   IO18   |
  |   MISO  |   D6    |   IO19   |
  |   MOSI  |   D7    |   IO23   |
  |   XRST  |   RST   |   EN     |
  |   CS    |   D1    |   IO5    |
  |   DCS   |   D0    |   IO16   |
  |   DREQ  |   D3    |   IO4    |
  |   5V    |   5V    |   5V     |
  |   GND   |   GND   |   GND    |
  --------------------------------

  Dependencies:
  -VS1053 library by baldram (https://github.com/baldram/ESP_VS1053_Library)
  -ESP8266Wifi/WiFi

  To run this example define the platformio.ini as below.

  [env:nodemcuv2]
  platform = espressif8266
  board = nodemcuv2
  framework = arduino
  build_flags = -D PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH
  lib_deps =
    ESP_VS1053_Library

  [env:esp32dev]
  platform = espressif32
  board = esp32dev
  framework = arduino
  lib_deps =
    ESP_VS1053_Library

  Instructions:
  -Build the hardware
    (please find an additional description and Fritzing's schematic here:
     https://github.com/vincegellar/Simple-Radio-Node#wiring)
  -Set the station in this file
  -Upload the program

  IDE Settings (Tools):
  -IwIP Variant: v1.4 Higher Bandwidth
  -CPU Frequency: 160Hz
*/

#include <VS1053.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D3
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4
#endif

// Default volume
#define VOLUME  80

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
WiFiClient client;

// WiFi settings example, substitute your own
const char* ssid = "TP-Link";
const char* password = "xxxxxxxx";
     
//  http://comet.shoutca.st:8563/1
const char *host = "comet.shoutca.st";
const char *path = "/1";
int httpPort = 8563;

// The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
uint8_t mp3buff[64];

void setup () {
    Serial.begin(115200);

    // Wait for VS1053 and PAM8403 to power up
    // otherwise the system might not start up correctly
    delay(3000);

    // This can be set in the IDE no need for ext library
    // system_update_cpu_freq(160);
    
    Serial.println("\n\nSimple Radio Node WiFi Radio");
    
    SPI.begin();

    player.begin();
    player.switchToMp3Mode();
    player.setVolume(VOLUME);

    Serial.print("Connecting to SSID "); Serial.println(ssid);
    WiFi.begin(ssid, password);
      
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("WiFi connected");  
    Serial.println("IP address: ");  Serial.println(WiFi.localIP());

    Serial.print("connecting to ");  Serial.println(host);
      
    if (!client.connect(host, httpPort)) {
      Serial.println("Connection failed");
      return;
    }

    Serial.print("Requesting stream: ");
    Serial.println(path);
    
    client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" + 
                  "Connection: close\r\n\r\n");
}

void loop() {
    if(!client.connected()){
      Serial.println("Reconnecting...");
      if(client.connect(host, httpPort)){
        client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" + 
                  "Connection: close\r\n\r\n");
      }
    }
  
    if(client.available() > 0){
      // The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
      uint8_t bytesread = client.read(mp3buff, 64);
      player.playChunk(mp3buff, bytesread);
    }
}
