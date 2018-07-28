/**
  A simple example to use ESP_VS1053_Library (plays a test sound every 3s)
  https://github.com/baldram/ESP_VS1053_Library
  If you like this project, please add a star.

  Copyright (C) 2018 Marcin Szalomski (github.com/baldram)
  Licensed under GNU GPL v3

  The circuit (example wiring for ESP8266 based board like eg. LoLin NodeMCU V3):
  ---------------------
  | VS1053  | ESP8266 |
  ---------------------
  |   SCK   |   D5    |
  |   MISO  |   D6    |
  |   MOSI  |   D7    |
  |   XRST  |   RST   |
  |   CS    |   D1    |
  |   DCS   |   D0    |
  |   DREQ  |   D3    |
  |   5V    |   VU    |
  |   GND   |   G     |
  ---------------------

  Note: It's just an example, you may use a different pins definition.
  For ESP32 example, please follow the link:
    https://github.com/baldram/ESP_VS1053_Library/issues/1#issuecomment-313907348

  To run this example define the platformio.ini as below.

  [env:nodemcuv2]
  platform = espressif8266
  board = nodemcuv2
  framework = arduino

  lib_deps =
    ESP_VS1053_Library

*/

// This ESP_VS1053_Library
#include <VS1053.h>

// Please find SampleMp3.h file here:
//   github.com/baldram/ESP_VS1053_Library/blob/master/examples/Mp3PlayerDemo/SampleMp3.h
#include "SampleMp3.h"

// Wiring of VS1053 board (SPI connected in a standard way)
#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D3

#define VOLUME  100 // volume level 0-100

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

void setup () {
    Serial.begin(115200);
  
    // initialize SPI
    SPI.begin();

    Serial.println("Hello VS1053!\n");
    // initialize a player
    player.begin();
    player.switchToMp3Mode(); // optional, some boards require this
    player.setVolume(VOLUME);
}

void loop() {
    Serial.println("Playing sound... ");
  
    // play mp3 flow each 3s
    player.playChunk(sampleMp3, sizeof(sampleMp3));
    delay(3000);
}
