# Esp-radio
Internet radio based on Esp8266 and VS1053.  Will compile in Arduino IDE.

Features:
-	Can connect to thousands of Internet radio stations that broadcast MP3 or Ogg audio streams.
-	Uses a minimal number of components; no Arduino required.
-	Handles bitrates up to 320 kbps.
-	Has a preset list of maximal 63 favorite radio stations in EEPROM.
-	Can be controlled by a tablet or other device through a build-in webserver.
-	Optional one or three button control to skip to the next preset station.
-	The strongest available WiFi network is automatically selected.
-	Heavily commented source code, easy to add extra functionality.
-	Debug information through serial output.
-	20 kB ring buffer to provide smooth playback.
-	SPIFFS filesystem used for website, WiFi SSIDs and passwords.
-	Software update over WiFi possible (OTA).
-	Saves volume and preset station over restart.
-	Bass and treble control.

See documentation in pdf-file.

Last changes:
- 04-jul-2016: WiFi.disconnect clears old connection now (thanks to Juppit)
- 27-may-2016: Fixed restore station at restart.
- 26-may-2016: Bugfix BUTTON3 handling (if no TFT).  Update pdf-document.
- 23-may-2016: Bugfix EEPROM handling.
- 17-may-2016: 3 button control over analog or digital input.
- 13-may-2016: Better detection of Ogg streams.
- 07-may-2016: Added selection of preset stations to sketch and web page.
- 06-may-2016: Added hidden SSID, added feature to web page.
- 04-may-2016, Allow stations like "skonto.ls.lv:8002/mp3".
- 03-may-2016, Add bass/treble settings (see also new index.html).
