# Esp-radio
Internet radio based on Esp8266 and VS1053.

Esp_radio -- Webradio receiver for ESP8266, 1.8 color display and VS1053 MP3 module.
With ESP8266 running at 80 MHz, it is capable of handling up to 256 kb bitrate.

 ESP8266 libraries used:
  - SPI
  - Adafruit_GFX
  - TFT_ILI9163C
 A library for the VS1053 (for ESP8266) is not available (or not easy to find).  Therefore
 a class for this module is derived from the maniacbug library and integrated in this sketch.

 See http://www.internet-radio.com for suitable stations.  Add the staions of your choice
 to the table "hostlist" in the global data secting further on.

 Brief description of the program:
 First a suitable WiFi network is found and a connection is made.
 Then a connection will be made to a shoutcast server.  The server starts with some
 info in the header in readable ascii, ending with a double CRLF, like:
 icy-name:Classic Rock Florida - SHE Radio
 icy-genre:Classic Rock 60s 70s 80s Oldies Miami South Florida
 icy-url:http://www.ClassicRockFLorida.com
 content-type:audio/mpeg
 icy-pub:1
 icy-metaint:32768          - Metadata after 32768 bytes of MP3-data
 icy-br:128                 - in kb/sec 

 After de double CRLF is received, the server starts sending mp3-data.  This data contains
 metadata (non mp3) after every "metaint" mp3 bytes.  This metadata is empty in most cases,
 but if any is available the content will be presented on the TFT.
 Pushing the input button causes the player to select the next station in the hostlist.

 The display used is a Chinese 1.8 color TFT module 128 x 160 pixels.  The TFT_ILI9163C.h
 file has been changed to reflect this particular module.  TFT_ILI9163C.cpp has been
 changed to use the full screenwidth if rotated to mode "3".  Now there is room for 26
 characters per line and 16 lines.  Software will work without installing the display.

 For configuration of the WiFi network(s): see the global data section further on.

 The SPI interface for VS1053 and TFT uses hardware SPI.

