# Esp-radio
Internet radio based on Esp8266 and VS1053.  Will compile in Arduino IDE.

Features:
-	Can connect to thousands of Internet radio stations that broadcast MP3 audio streams.
-	Uses a minimal number of components; no Arduino required.
-	Handles bitrates up to 320 kbps.
-	Has a preset list of maximal 63 favorite radio stations in EEPROM.
-	Can be controlled by a tablet or other device through a build-in webserver.
-	Optional one button control to skip to the next preset station.
-	The strongest available WiFi network is automatically selected.
-	Heavily commented source code, easy to add extra functionality.
-	Debug information through serial output.
-	Big ring buffer to provide smooth playback.
-	SPIFFS filesystem used for website, WiFi SSIDs and passwords.
-	Software update over WiFi possible (OTA).
-	Saves volume and preset station over restart.
-	Bass and treble control.

See documentation in pdf-file.

Last changes:
07-may-2016: Added selection of preset stations to sketch and web page.
06-may-2016: Added hidden SSID, added feature to web page.
04-may-2016, Allow stations like "skonto.ls.lv:8002/mp3".
03-may-2016, Add bass/treble settings (see also new index.html).
