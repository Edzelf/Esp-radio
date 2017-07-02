# VS1053 library

This is a library for the generic **VS1053 Codec Breakout**.
A powerful MP3 / ACC / WMA decoder.

The library is a great base to build your own webradio player.

Designed specifically to work with the **Espressif ESP8266 and ESP32** boards. 

There are currently two methods to program the ESP boards: the ESP-IDF and the ESP8266/ESP32 arduino Core.
The library was created to work with the **arduino Core**.
 
The ESP8266 is the most popular Wi-Fi MCU (known also as ESP12, NodeMCU, WeMos, ...). 
But the library should also work with classic Arduino Uno board too.

## Usage 

To use this library in your PlatformIO project, simply add to your `platformio.ini` a dependency as following:

```
lib_deps =
    baldram/ESP_VS1053_Library
```

From your `.cpp` code instantiate on object of VS1053 player as below:

```
VS1053 player(CS, DCS, DREQ);
```

Then initialize player and use as in following example:

```
player.begin();
player.setVolume(MAXIMUM_VOLUME);
player.playChunk(helloMp3, sizeof(helloMp3));
```

For complete code please check `examples` folder.
