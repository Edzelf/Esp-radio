# VS1053 library

This is a library for the generic **VS1053 Breakout**.
A powerful Ogg Vorbis / MP3 / AAC / WMA / FLAC / MIDI Audio Codec chip.

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
player.setVolume(VOLUME);
player.playChunk(helloMp3, sizeof(helloMp3));
```

For complete code please check `examples` folder.

## Example wiring

An example for ESP8266 based board like eg. LoLin NodeMCU V3 or WeMos D1 R2.

```

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
  
```

## Credits

Based on library/applications:
* [maniacbug/VS1053](https://github.com/maniacbug/VS1053) by [J. Coliz](https://github.com/maniacbug)
* [Esp-radio](https://github.com/Edzelf/Esp-radio) by [Ed Smallenburg](https://github.com/Edzelf)
* [smart-pod](https://github.com/MagicCube/smart-pod) by [Henry Li](https://github.com/MagicCube)

## License

Copyright (C) 2017

Licensed under GNU GPLv3
