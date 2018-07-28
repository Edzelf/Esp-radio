# VS1053 library

This is a PlatformIO library for the generic **[VS1053 Breakout](http://www.vlsi.fi/en/products/vs1053.html)** by VLSI Solution.<br/>
A powerful Ogg Vorbis / MP3 / AAC / WMA / FLAC / MIDI Audio Codec chip.<br/>
Read more: [http://www.vlsi.fi/en/products/vs1053.html](http://www.vlsi.fi/en/products/vs1053.html).

The library may be a base to build your own webradio player or different audio device.

Designed specifically to work with the **Espressif ESP8266** and **ESP32** boards. 

Don't hesitate to create issues or pull requests if you want to improve ESP_VS1053_Library!

## Introduction

#### How to use the library?

There are currently two official methods to program the ESP boards: the ESP-IDF and the ESP8266/ESP32 arduino Core.
The library was created to work with the **arduino Core**.

The ESP8266 is the most popular Wi-Fi MCU (known also as ESP12, NodeMCU, WeMos, ...). 

#### Why PlatformIO?

As mentioned in first paragraph, the library was prepared to be used with PlatformIO, which is kind of dependency management tool. It is highly recommended environment to be used while developing your electronics/IoT projects. The build automation lets you deal much easier while working with the project. It will help you to have all project dependencies simply resolved out of the box. What more, it provides very good IDE integration (any of popular), debugging possibility, remote unit testing and firmware updates. [Learn more about PlatformIO here](https://platformio.org/).

## Usage 

#### Configure and use the library

To use this library in your PlatformIO project, simply add to your `platformio.ini` a dependency (id=1744) as following:

```
lib_deps =
    ESP_VS1053_Library
```

From your `.cpp` or `.ino` code include VS1053 library.

```
#include <VS1053.h>
```

Afterwards simply instantiate an object of VS1053 player as below:

```
VS1053 player(CS, DCS, DREQ);
```

Then initialize the player and use as in following example:

```
player.begin();
player.setVolume(VOLUME);
player.switchToMp3Mode();
player.playChunk(sampleMp3, sizeof(sampleMp3));
```
    
For complete code please check [examples](https://github.com/baldram/ESP_VS1053_Library/tree/master/examples) folder.
The example plays the sound like this [(click to listen to the sound)](https://drive.google.com/open?id=1Mm4dc-sM7KjZcKmv5g1nwhe3-qtm7yUl) every three minutes.

Please note that `player.switchToMp3Mode()` is an optional switch. Some of VS1053 modules will start up in MIDI mode. The result is no audio when playing MP3.
You can modify the board, but there is a more elegant way without soldering. For more details please read a discussion here: [http://www.bajdi.com/lcsoft-vs1053-mp3-module/#comment-33773](http://www.bajdi.com/lcsoft-vs1053-mp3-module/#comment-33773).
<br />No side effects for boards which do not need this switch, so you can call it just in case.

#### Logging / debugging

The library uses ESP Arduino framework built in logger (Arduino core for [ESP32](https://github.com/espressif/arduino-esp32/issues/893#issuecomment-348069135) and [ESP8266](https://github.com/esp8266/Arduino/blob/master/doc/Troubleshooting/debugging.rst#debug-level)).<br /> 

To see debug messages please add build flags to your `platformio.ini` as below (depending on platform):

- for ESP8266:

`build_flags = -D DEBUG_ESP_PORT=Serial`

- for ESP32:

`build_flags = -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG`

The Serial Interface needs to be initialized in the `setup()`.

```
void setup() {
    Serial.begin(115200);
}
```
Now if something is wrong, you'll see the output like below (from ESP32):

```
[I][main.cpp:117] setup(): Hello # VS1053!
[D][VS1053.cpp:156] begin(): 
[D][VS1053.cpp:157] begin(): Reset VS1053...
[D][VS1053.cpp:161] begin(): End reset VS1053...
[D][VS1053.cpp:119] testComm(): VS1053 not properly installed!
```
In successful case it would start with something like this:

```
[I][main.cpp:117] setup(): Hello # VS1053!
[D][VS1053.cpp:156] begin(): 
[D][VS1053.cpp:157] begin(): Reset VS1053...
[D][VS1053.cpp:161] begin(): End reset VS1053...
[D][VS1053.cpp:132] testComm(): Slow SPI,Testing VS1053 read/write registers...
[D][VS1053.cpp:132] testComm(): Fast SPI, Testing VS1053 read/write registers again...
[D][VS1053.cpp:183] begin(): endFillByte is 0
```

## Example wiring

An example for ESP8266 based board like eg. LoLin NodeMCU V3 or WeMos D1 R2.

|  VS1053  | ESP8266  |
|----------|----------|
| SCK      | D5       |
| MISO     | D6       |
| MOSI     | D7       |
| XRST     | RST      |
| CS       | D1       |
| DCS      | D0       |
| DREQ     | D3       |
| 5V       | VU       |
| GND      | G        |

For ESP32 and other boards wiring examples please see thread [Supported hardware](https://github.com/baldram/ESP_VS1053_Library/issues/1) and [Tested boards](https://github.com/baldram/ESP_VS1053_Library/blob/master/doc/tested-boards.md) list.

<img alt="VS1053B and NodeMCU v3" title="VS1053B and NodeMCU v3" src="https://user-images.githubusercontent.com/16861531/27875071-3ead1674-61b2-11e7-9a69-02edafa7b286.jpg" width="300px" />

## Library developement

**Please feel invited** to provide your pull request with improvement or a bug fix.

A hint for CLion developers.
The IDE files are added to `.gitignore`, but once you clone the code you should be able to develop the 
library easily with CLion after calling the command from terminal as below:

```
$ platformio init --ide=clion
```
Then please import the project and run the PIO task: `PLATFORMIO_REBUILD_PROJECT_INDEX`.<br />
Read more here: [PlatformIO & CLion integration](http://docs.platformio.org/en/latest/ide/clion.html).

## Credits

Based on library/applications:
* [maniacbug/VS1053](https://github.com/maniacbug/VS1053) by [J. Coliz](https://github.com/maniacbug)
* [Esp-radio](https://github.com/Edzelf/Esp-radio) by [Ed Smallenburg](https://github.com/Edzelf)
* [smart-pod](https://github.com/MagicCube/smart-pod) by [Henry Li](https://github.com/MagicCube)

## License

Copyright (C) 2017<br/>
Licensed under GNU GPLv3
