# Esp-radio
Internet radio based on Esp8266 and VS1053.  Will compile in Arduino IDE.  New version 24-may-2017.

NOTES:
- If you are using V2.4.0 of the core library: set IwIP Variant to "V1.4 Prebuilt" in the Tools of the IDE.
- The radio will NOT play AACP streams.

Features:
-	Can connect to thousands of Internet radio stations that broadcast MP3 or Ogg audio streams.
- Can connect to a standalone mp3 file on a server.
- Can connect to a local mp3 file on SPIFFS.
- Support for .m3u playlists.
-	Uses a minimal number of components; no Arduino required.
-	Handles bitrates up to 320 kbps.
-	Has a preset list of maximal 100 favorite radio stations in configuration file.
- Configuration file can be edited through web interface.
-	Can be controlled by a tablet or other device through a build-in webserver.
- Can be controlled over MQTT.
- Can be controlled over Serial Input.
-	Optional one or three button control to skip to the next preset station.
-	The strongest available WiFi network is automatically selected.
-	Heavily commented source code, easy to add extra functionality.
-	Debug information through serial output.
-	20 kB ring buffer to provide smooth playback.
-	SPIFFS filesystem used for configuration of WiFi SSIDs, passwords and small MP3-files.
-	Software update over WiFi possible (OTA).
-	Saves volume and preset station over restart.
-	Bass and treble control.
- Configuration also possible if no WiFi connection can be established.
- Can play iHeartRadio stations.

See documentation in pdf-file.

Last changes:
- 10-feb-2022: Add redirection.
- 05-apr-2018: Fixed crash when no known WiFi network was found.
- 18-apr-2018: Work-around for wifi.connected() bug.
- 31-may-2017: Volume indicator on display.
- 26-may-2017: Correction playing .m3u playlists.
- 24-may-2017: Correction. Do not skip first part of .mp3 file.
- 11-may-2017: Convert UTF8 characters before display, thanks to everyb313.
- 09-may-2017: Fixed issue on analog input.
- 04-may-2017: Integrate iHeartRadio, thanks to NonaSuomy
- 03-may-2017: Prevent to start inputstream if no network.
- 26-feb-2017: Better output webinterface on preset change.
- 01-feb-2017: Bugfix uploading files.
- 30-jan-2017: Allow chunked transfer encoding of streams.
- 23-jan-2017: Correction playlists.
- 16-jan-2017: Correction playlists.
- 02-jan-2017: Webinterface in PROGMEM.
- 28-dec-2016: Add support for resume after stop.
- 23-dec-2016: Add support for mp3 files on SPIFFS.
- 15-nov-2016: Support for .m3u files.
- 22-oct-2016: Correction mute/unmute.
- 14-oct-2016: Update for AsyncMqttClient version 0.5.0. Added extra documentation for MQTT.
- 11-oct-2016: Allow stations that do not specify bitrate.  Allow standalone MP3s. 
- 04-oct-2016: Version with MQTT and configuration in radio.ini file.
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

