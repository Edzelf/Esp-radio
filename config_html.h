// config.html file in raw data format for PROGMEM
//
const char config_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
 <head>
  <title>Configuration ESP-radio</title>
  <meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">
  <link rel="Shortcut Icon" type="image/ico" href="favicon.ico">
 </head>
 <body>
  <ul>
   <li><a class="pull-left" href="#">ESP Radio</a></li>
   <li><a class="pull-left" href="/index.html">Control</a></li>
   <li><a class="pull-left active" href="/config.html">Config</a></li>
   <li><a class="pull-left" href="/about.html">About</a></li>
  </ul>
  <br><br><br>
  <center>
   <h1>** ESP Radio **</h1>
   <p>You can edit the configuration here. <i>Note that this will be effective on the next restart of the Esp-radio.</i></p>
   <h4>Available WiFi networks</h4>
   <select class="select" onChange="handletone(this)" id="ssid"></select>
   <br><br>
   <textarea rows="25" cols="100" id="inifile">Space for inifile</textarea> 
   <br><br>
   <button class="button" onclick="fsav()">Save</button>
   &nbsp;&nbsp;
   <button class="button buttonr" onclick="httpGet('reset')">Restart</button>
   <form action="#" onsubmit="return uploadfile(this);" enctype="multipart/form-data" method="post" name="fileinfo">
     <h4>Upload file:</h4>
     <input type="file" name="file" size="50"><br>
     <input type="submit" value="Send">
    </form>
    <br><input type="text" size="80" id="resultstr" placeholder="Waiting for input....">
    <br><br>

    <script>
      function httpGet ( theReq )
      {
        var theUrl = "/?" + theReq + "&version=" + Math.random() ;
        var xhr = new XMLHttpRequest() ;
        xhr.onreadystatechange = function() {
          if ( xhr.readyState == XMLHttpRequest.DONE )
          {
            resultstr.value = xhr.responseText ;
          }
        }
        xhr.open ( "GET", theUrl, false ) ;
        xhr.send() ;
      }


      function fsav()
      {
        var theUrl = "/?save=0" ;
        var xhr = new XMLHttpRequest() ;
        xhr.onreadystatechange = function() {
          if ( xhr.readyState == XMLHttpRequest.DONE )
          {
            resultstr.value = xhr.responseText ;
          }
        }
        xhr.open ( "POST", theUrl, true ) ;
        xhr.setRequestHeader ( "Content-type", "application/x-www-form-urlencoded" ) ;
        xhr.send ( "content=" + inifile.value ) ;
      }


      function uploadfile ( theForm )
      {
        var oData, oReq ;

        oData = new FormData ( fileinfo ) ;
        oReq = new XMLHttpRequest() ;

        oReq.open ( "POST", "/upload", true ) ;
        oReq.onload = function ( oEvent ) {
          if ( oReq.status == 200 )
          {
            resultstr.value = oReq.responseText ;
          }
          else
          {
            resultstr.value = "Error " + oReq.statusText ;
          }
        }
        oReq.send ( oData ) ;
        return false ;
      }

      // Fill configuration initially
      // First the available WiFi networks
      var i, select, opt, networks, params ;

      select = document.getElementById("selnet") ;
      var theUrl = "/?getnetworks=0" + "&version=" + Math.random() ;
      var xhr = new XMLHttpRequest() ;
      xhr.onreadystatechange = function() {
        if ( xhr.readyState == XMLHttpRequest.DONE )
        {
          networks = xhr.responseText.split ( "|" ) ;
          
          for ( i = 0 ; i < ( networks.length - 1 ) ; i++ )
          {
            opt = document.createElement( "OPTION" ) ;
            opt.value = i ;
            opt.text = networks[i] ;
            ssid.add( opt ) ;
          }
        }
      }
      xhr.open ( "GET", theUrl, false ) ;
      xhr.send() ;

      // Now get the configuration parameters from radio.ini
      theUrl = "/radio.ini" ;
      xhr.onreadystatechange = function() {
        if ( xhr.readyState == XMLHttpRequest.DONE )
        {
          inifile.value = xhr.responseText ;
        }
      }
      xhr.open ( "GET", theUrl, false ) ;
      xhr.send() ;
    </script>
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
