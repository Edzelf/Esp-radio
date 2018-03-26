// about.html file in raw data format for PROGMEM
//
const char about_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
 <head>
  <title>Info Page</title>
  <meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">
  <link rel="Shortcut Icon" type="image/ico" href="favicon.ico">
 </head>
 <body>
  <ul>
   <li><a class="pull-left-shift" href="index.html">Control</a></li>
   <li><a class="pull-left" href="config.html">Config</a></li>
   <li><a class="pull-left active" href="about.html">About</a></li>
  </ul>
  <br><br><br>
  <center>
   <h1>Internet Radio Player Info Page</h1>
	<p>Internet Radio Player based on ESP8266, 1.8" TFT display and VS1053 MP3/AAC Decoder.<br>
	This project is documented at <a target="blank" href="https://github.com/Edzelf/Esp-radio">Github</a>.</p>
	<p>Author: Ed Smallenburg, Jan Grulich<br>
	Webinterface design: <a target="blank" href="http://www.sanderjochems.nl/">Sander Jochems</a><br>
	App (Android): <a target="blank" href="https://play.google.com/store/apps/details?id=com.thunkable.android.sander542jochems.ESP_Radio">Sander Jochems</a><br>
	Date: January 2018</p>
  </center>
  <script type="text/javascript">
    var stylesheet = document.createElement('link') ;
    stylesheet.href = 'radio.css' ;
    stylesheet.rel = 'stylesheet' ;
    stylesheet.type = 'text/css' ;
    document.getElementsByTagName('head')[0].appendChild(stylesheet) ;
  </script>
 </body>
</html>
)=====" ;
