//******************************************************************************************
//*  Esp_radio -- Webradio receiver for ESP8266, (color) display and VS1053 MP3 module,    *
//*               by Ed Smallenburg (ed@smallenburg.nl)                                    *
//*  With ESP8266 running at 80 MHz, it is capable of handling up to 256 kb bitrate.       *
//*  With ESP8266 running at 160 MHz, it is capable of handling up to 320 kb bitrate.      *
//******************************************************************************************
// ESP8266 libraries used:
//  - ESP8266WiFi       - Part of ESP8266 Arduino default libraries.
//  - SPI               - Part of Arduino default libraries.
//  - Adafruit_GFX      - https://github.com/adafruit/Adafruit-GFX-Library
//  - TFT_ILI9163C      - https://github.com/sumotoy/TFT_ILI9163C
//  - ESPAsyncTCP       - https://github.com/me-no-dev/ESPAsyncTCP
//  - ESPAsyncWebServer - https://github.com/me-no-dev/ESPAsyncWebServer
//  - FS - https://github.com/esp8266/arduino-esp8266fs-plugin/releases/download/0.2.0/ESP8266FS-0.2.0.zip
//  - ArduinoOTA        - Part of ESP8266 Arduino default libraries.
//  - AsyncMqttClient   - https://github.com/marvinroger/async-mqtt-client
//  - TinyXML           - Fork https://github.com/adafruit/TinyXML
//
// A library for the VS1053 (for ESP8266) is not available (or not easy to find).  Therefore
// a class for this module is derived from the maniacbug library and integrated in this sketch.
//
// See http://www.internet-radio.com for suitable stations.  Add the stations of your choice
// to the .ini-file.
//
// Brief description of the program:
// First a suitable WiFi network is found and a connection is made.
// Then a connection will be made to a shoutcast server.  The server starts with some
// info in the header in readable ascii, ending with a double CRLF, like:
//  icy-name:Classic Rock Florida - SHE Radio
//  icy-genre:Classic Rock 60s 70s 80s Oldies Miami South Florida
//  icy-url:http://www.ClassicRockFLorida.com
//  content-type:audio/mpeg
//  icy-pub:1
//  icy-metaint:32768          - Metadata after 32768 bytes of MP3-data
//  icy-br:128                 - in kb/sec (for Ogg this is like "icy-br=Quality 2"
//
// After de double CRLF is received, the server starts sending mp3- or Ogg-data.  For mp3, this
// data may contain metadata (non mp3) after every "metaint" mp3 bytes.
// The metadata is empty in most cases, but if any is available the content will be presented on the TFT.
// Pushing the input button causes the player to select the next preset station present in the .ini file.
//
// The display used is a Chinese 1.8 color TFT module 128 x 160 pixels.  The TFT_ILI9163C.h
// file has been changed to reflect this particular module.  TFT_ILI9163C.cpp has been
// changed to use the full screenwidth if rotated to mode "3".  Now there is room for 26
// characters per line and 16 lines.  Software will work without installing the display.
// If no TFT is used, you may use GPIO2 and GPIO15 as control buttons.  See definition of "USETFT" below.
// Switches are than programmed as:
// GPIO2 : "Goto station 1"
// GPIO0 : "Next station"
// GPIO15: "Previous station".  Note that GPIO15 has to be LOW when starting the ESP8266.
//         The button for GPIO15 must therefore be connected to VCC (3.3V) instead of GND.

//
// For configuration of the WiFi network(s): see the global data section further on.
//
// The SPI interface for VS1053 and TFT uses hardware SPI.
//
// Wiring:
// NodeMCU  GPIO    Pin to program  Wired to LCD        Wired to VS1053      Wired to rest
// -------  ------  --------------  ---------------     -------------------  ---------------------
// D0       GPIO16  16              -                   pin 1 DCS            -
// D1       GPIO5    5              -                   pin 2 CS             LED on nodeMCU
// D2       GPIO4    4              -                   pin 4 DREQ           -
// D3       GPIO0    0 FLASH        -                   -                    Control button "Next station"
// D4       GPIO2    2              pin 3 (D/C)         -                    (OR)Control button "Station 1"
// D5       GPIO14  14 SCLK         pin 5 (CLK)         pin 5 SCK            -
// D6       GPIO12  12 MISO         -                   pin 7 MISO           -
// D7       GPIO13  13 MOSI         pin 4 (DIN)         pin 6 MOSI           -
// D8       GPIO15  15              pin 2 (CS)          -                    (OR)Control button "Previous station"
// D9       GPI03    3 RXD0         -                   -                    Reserved serial input
// D10      GPIO1    1 TXD0         -                   -                    Reserved serial output
// -------  ------  --------------  ---------------     -------------------  ---------------------
// GND      -        -              pin 8 (GND)         pin 8 GND            Power supply
// VCC 3.3  -        -              pin 6 (VCC)         -                    LDO 3.3 Volt
// VCC 5 V  -        -              pin 7 (BL)          pin 9 5V             Power supply
// RST      -        -              pin 1 (RST)         pin 3 RESET          Reset circuit
//
// The reset circuit is a circuit with 2 diodes to GPIO5 and GPIO16 and a resistor to ground
// (wired OR gate) because there was not a free GPIO output available for this function.
// This circuit is included in the documentation.
// Issues:
// Webserver produces error "LmacRxBlk:1" after some time.  After that it will work very slow.
// The program will reset the ESP8266 in such a case.  Now we have switched to async webserver,
// the problem still exists, but the program will not crash anymore.
// Upload to ESP8266 not reliable.
//
// 31-03-2016, ES: First set-up.
// 01-04-2016, ES: Detect missing VS1053 at start-up.
// 05-04-2016, ES: Added commands through http server on port 80.
// 14-04-2016, ES: Added icon and switch preset on stream error.
// 18-04-2016, ES: Added SPIFFS for webserver.
// 19-04-2016, ES: Added ringbuffer.
// 20-04-2016, ES: WiFi Passwords through SPIFFS files, enable OTA.
// 21-04-2016, ES: Switch to Async Webserver.
// 27-04-2016, ES: Save settings, so same volume and preset will be used after restart.
// 03-05-2016, ES: Add bass/treble settings (see also new index.html).
// 04-05-2016, ES: Allow stations like "skonto.ls.lv:8002/mp3".
// 06-05-2016, ES: Allow hiddens WiFi station if this is the only .pw file.
// 07-05-2016, ES: Added preset selection in webserver.
// 12-05-2016, ES: Added support for Ogg-encoder.
// 13-05-2016, ES: Better Ogg detection.
// 17-05-2016, ES: Analog input for commands, extra buttons if no TFT required.
// 26-05-2016, ES: Fixed BUTTON3 bug (no TFT).
// 27-05-2016, ES: Fixed restore station at restart.
// 04-07-2016, ES: WiFi.disconnect clears old connection now (thanks to Juppit).
// 23-09-2016, ES: Added commands via MQTT and Serial input, Wifi set-up in AP mode.
// 04-10-2016, ES: Configuration in .ini file. No more use of EEPROM and .pw files.
// 11-10-2016, ES: Allow stations that have no bitrate in header like icecast.err.ee/raadio2.mp3.
// 14-10-2016, ES: Updated for async-mqtt-client-master 0.5.0
// 22-10-2016, ES: Correction mute/unmute.
// 15-11-2016, ES: Support for .m3u playlists.
// 22-12-2016, ES: Support for localhost (play from SPIFFS).
// 28-12-2016, ES: Implement "Resume" request.
// 31-12-2016, ES: Allow ContentType "text/css".
// 02-01-2017, ES: Webinterface in PROGMEM.
// 16-01-2017, ES: Correction playlists.
// 17-01-2017, ES: Bugfix config page and playlist.
// 23-01-2017, ES: Bugfix playlist.
// 26-01-2017, ES: Check on wrong icy-metaint.
// 30-01-2017, ES: Allow chunked transfer encoding.
// 01-02-2017, ES: Bugfix file upload.
// 26-04-2017, ES: Better output webinterface on preset change.
// 03-05-2017, ES: Prevent to start inputstream if no network.
// 04-05-2017, ES: Integrate iHeartRadio, thanks to NonaSuomy.
// 09-05-2017, ES: Fixed abs problem.
// 11-05-2017, ES: Convert UTF8 characters before display, thanks to everyb313.
// 24-05-2017, ES: Correction. Do not skip first part of .mp3 file.
// 26-05-2017, ES: Correction playing from .m3u playlist and LC/UC problem.
// 31-05-2017, ES: Volume indicator on TFT.
// 02-02-2018, ES: Force 802.11N connection.
// 18-04-2018, ES: Workaround for not working wifi.connected().
// 05-10-2018, ES: Fixed exception if no network was found.
//
// Define the version number, also used for webserver as Last-Modified header:
#define VERSION "Fri, 05 Oct 2018 09:30:00 GMT"
// TFT.  Define USETFT if required.
#define USETFT
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncMqttClient.h>
#include <SPI.h>
#if defined ( USETFT )
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#endif
#include <Ticker.h>
#include <stdio.h>
#include <string.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <TinyXML.h>

extern "C"
{
#include "user_interface.h"
}

// Definitions for 3 control switches on analog input
// You can test the analog input values by holding down the switch and select /?analog=1
// in the web interface. See schematics in the documentation.
// Switches are programmed as "Goto station 1", "Next station" and "Previous station" respectively.
// Set these values to 2000 if not used or tie analog input to ground.
#define NUMANA  3
//#define asw1    252
//#define asw2    334
//#define asw3    499
#define asw1    2000
#define asw2    2000
#define asw3    2000
//
// Color definitions for the TFT screen (if used)
#define BLACK   0x0000
#define BLUE    0xF800
#define RED     0x001F
#define GREEN   0x07E0
#define CYAN    GREEN | BLUE
#define MAGENTA RED | BLUE
#define YELLOW  RED | GREEN
// Digital I/O used
// Pins for VS1053 module
#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4
// Pins CS and DC for TFT module (if used, see definition of "USETFT")
#define TFT_CS 15
#define TFT_DC 2
// Control button (GPIO) for controlling station
#define BUTTON1 2
#define BUTTON2 0
#define BUTTON3 15
// Ringbuffer for smooth playing. 20000 bytes is 160 Kbits, about 1.5 seconds at 128kb bitrate.
#define RINGBFSIZ 20000
// Debug buffer size
#define DEBUG_BUFFER_SIZE 100
// Name of the ini file
#define INIFILENAME "/radio.ini"
// Access point name if connection to WiFi network fails.  Also the hostname for WiFi and OTA.
// Not that the password of an AP must be at least as long as 8 characters.
// Also used for other naming.
#define NAME "Esp-radio"
// Maximum number of MQTT reconnects before give-up
#define MAXMQTTCONNECTS 20
//
//******************************************************************************************
// Forward declaration of various functions                                                *
//******************************************************************************************
//void   displayinfo ( const char* str, uint16_t pos, uint16_t height, uint16_t color ) ;
void   showstreamtitle ( const char* ml, bool full = false ) ;
void   handlebyte ( uint8_t b, bool force = false ) ;
void   handlebyte_ch ( uint8_t b, bool force = false ) ;
void   handleFS ( AsyncWebServerRequest* request ) ;
void   handleFSf ( AsyncWebServerRequest* request, const String& filename ) ;
void   handleCmd ( AsyncWebServerRequest* request )  ;
void   handleFileUpload ( AsyncWebServerRequest* request, String filename,
                          size_t index, uint8_t* data, size_t len, bool final ) ;
char*  dbgprint( const char* format, ... ) ;
char*  analyzeCmd ( const char* str ) ;
char*  analyzeCmd ( const char* par, const char* val ) ;
String chomp ( String str ) ;
void   publishIP() ;
String xmlparse ( String mount ) ;
void   put_eeprom_station ( int index, const char *entry ) ;
char*  get_eeprom_station ( int index ) ;
int    find_eeprom_station ( const char *search_entry ) ;
int    find_free_eeprom_entry() ;
bool   connecttohost() ;


//
//******************************************************************************************
// Global data section.                                                                    *
//******************************************************************************************
// There is a block ini-data that contains some configuration.  Configuration data is      *
// saved in the SPIFFS file radio.ini by the webinterface.  On restart the new data will   *
// be read from this file.                                                                 *
// Items in ini_block can be changed by commands from webserver/MQTT/Serial.               *
//******************************************************************************************
struct ini_struct
{
  String         mqttbroker ;                              // The name of the MQTT broker server
  uint16_t       mqttport ;                                // Port, default 1883
  String         mqttuser ;                                // User for MQTT authentication
  String         mqttpasswd ;                              // Password for MQTT authentication
  String         mqtttopic ;                               // Topic to suscribe to
  String         mqttpubtopic ;                            // Topic to pubtop (IP will be published)
  uint8_t        reqvol ;                                  // Requested volume
  uint8_t        rtone[4] ;                                // Requested bass/treble settings
  int8_t         newpreset ;                               // Requested preset
  String         ssid ;                                    // SSID of WiFi network to connect to
  String         passwd ;                                  // Password for WiFi network
} ;

enum datamode_t { INIT = 1, HEADER = 2, DATA = 4,
                  METADATA = 8, PLAYLISTINIT = 16,
                  PLAYLISTHEADER = 32, PLAYLISTDATA = 64,
                  STOPREQD = 128, STOPPED = 256
                } ;        // State for datastream

// Global variables
int              DEBUG = 1 ;
ini_struct       ini_block ;                               // Holds configurable data
WiFiClient       *mp3client = NULL ;                       // An instance of the mp3 client
AsyncWebServer   cmdserver ( 80 ) ;                        // Instance of embedded webserver on port 80
AsyncMqttClient  mqttclient ;                              // Client for MQTT subscriber
IPAddress        mqtt_server_IP ;                          // IP address of MQTT broker
char             cmd[130] ;                                // Command from MQTT or Serial
#if defined ( USETFT )
TFT_ILI9163C     tft = TFT_ILI9163C ( TFT_CS, TFT_DC ) ;
#endif
Ticker           tckr ;                                    // For timing 100 msec
TinyXML          xml;                                      // For XML parser.
uint32_t         totalcount = 0 ;                          // Counter mp3 data
datamode_t       datamode ;                                // State of datastream
int              metacount ;                               // Number of bytes in metadata
int              datacount ;                               // Counter databytes before metadata
String           metaline ;                                // Readable line in metadata
String           icystreamtitle ;                          // Streamtitle from metadata
String           icyname ;                                 // Icecast station name
int              bitrate ;                                 // Bitrate in kb/sec
int              metaint = 0 ;                             // Number of databytes between metadata
int8_t           currentpreset = -1 ;                      // Preset station playing
String           host ;                                    // The URL to connect to or file to play
String           playlist ;                                // The URL of the specified playlist
bool             xmlreq = false ;                          // Request for XML parse.
bool             hostreq = false ;                         // Request for new host
bool             reqtone = false ;                         // New tone setting requested
bool             muteflag = false ;                        // Mute output
uint8_t*         ringbuf ;                                 // Ringbuffer for VS1053
uint16_t         rbwindex = 0 ;                            // Fill pointer in ringbuffer
uint16_t         rbrindex = RINGBFSIZ - 1 ;                // Emptypointer in ringbuffer
uint16_t         rcount = 0 ;                              // Number of bytes in ringbuffer
uint16_t         analogsw[NUMANA] = { asw1, asw2, asw3 } ; // 3 levels of analog input
uint16_t         analogrest ;                              // Rest value of analog input
bool             resetreq = false ;                        // Request to reset the ESP8266
bool             NetworkFound ;                            // True if WiFi network connected
String           networks ;                                // Found networks
String           anetworks ;                               // Aceptable networks (present in .ini file)
String           presetlist ;                              // List for webserver
uint8_t          num_an ;                                  // Number of acceptable networks in .ini file
String           testfilename = "" ;                       // File to test (SPIFFS speed)
uint16_t         mqttcount = 0 ;                           // Counter MAXMQTTCONNECTS
int8_t           playlist_num = 0 ;                        // Nonzero for selection from playlist
File             mp3file  ;                                // File containing mp3 on SPIFFS
bool             localfile = false ;                       // Play from local mp3-file or not
bool             chunked = false ;                         // Station provides chunked transfer
int              chunkcount = 0 ;                          // Counter for chunked transfer

// XML parse globals.
const char* xmlhost = "playerservices.streamtheworld.com" ;// XML data source
const char* xmlget =  "GET /api/livestream"                // XML get parameters
                      "?version=1.5"                       // API Version of IHeartRadio
                      "&mount=%sAAC"                       // MountPoint with Station Callsign
                      "&lang=en" ;                         // Language
int         xmlport = 80 ;                                 // XML Port
uint8_t     xmlbuffer[150] ;                               // For XML decoding
String      xmlOpen ;                                      // Opening XML tag
String      xmlTag ;                                       // Current XML tag
String      xmlData ;                                      // Data inside tag
String      stationServer( "" ) ;                          // Radio stream server
String      stationPort( "" ) ;                            // Radio stream port
String      stationMount( "" ) ;                           // Radio stream Callsign

//******************************************************************************************
// End of global data section.                                                             *
//******************************************************************************************

//******************************************************************************************
// Pages and CSS for the webinterface.                                                     *
//******************************************************************************************
#include "about_html.h"
#include "config_html.h"
#include "index_html.h"
#include "radio_css.h"
#include "favicon_ico.h"
//
//******************************************************************************************
// VS1053 stuff.  Based on maniacbug library.                                              *
//******************************************************************************************
// VS1053 class definition.                                                                *
//******************************************************************************************
class VS1053
{
  private:
    uint8_t       cs_pin ;                        // Pin where CS line is connected
    uint8_t       dcs_pin ;                       // Pin where DCS line is connected
    uint8_t       dreq_pin ;                      // Pin where DREQ line is connected
    uint8_t       curvol ;                        // Current volume setting 0..100%
    const uint8_t vs1053_chunk_size = 32 ;
    // SCI Register
    const uint8_t SCI_MODE          = 0x0 ;
    const uint8_t SCI_BASS          = 0x2 ;
    const uint8_t SCI_CLOCKF        = 0x3 ;
    const uint8_t SCI_AUDATA        = 0x5 ;
    const uint8_t SCI_WRAM          = 0x6 ;
    const uint8_t SCI_WRAMADDR      = 0x7 ;
    const uint8_t SCI_AIADDR        = 0xA ;
    const uint8_t SCI_VOL           = 0xB ;
    const uint8_t SCI_AICTRL0       = 0xC ;
    const uint8_t SCI_AICTRL1       = 0xD ;
    const uint8_t SCI_num_registers = 0xF ;
    // SCI_MODE bits
    const uint8_t SM_SDINEW         = 11 ;        // Bitnumber in SCI_MODE always on
    const uint8_t SM_RESET          = 2 ;         // Bitnumber in SCI_MODE soft reset
    const uint8_t SM_CANCEL         = 3 ;         // Bitnumber in SCI_MODE cancel song
    const uint8_t SM_TESTS          = 5 ;         // Bitnumber in SCI_MODE for tests
    const uint8_t SM_LINE1          = 14 ;        // Bitnumber in SCI_MODE for Line input
    SPISettings   VS1053_SPI ;                    // SPI settings for this slave
    uint8_t       endFillByte ;                   // Byte to send when stopping song
  protected:
    inline void await_data_request() const
    {
      while ( !digitalRead ( dreq_pin ) )
      {
        yield() ;                                 // Very short delay
      }
    }

    inline void control_mode_on() const
    {
      SPI.beginTransaction ( VS1053_SPI ) ;       // Prevent other SPI users
      digitalWrite ( dcs_pin, HIGH ) ;            // Bring slave in control mode
      digitalWrite ( cs_pin, LOW ) ;
    }

    inline void control_mode_off() const
    {
      digitalWrite ( cs_pin, HIGH ) ;             // End control mode
      SPI.endTransaction() ;                      // Allow other SPI users
    }

    inline void data_mode_on() const
    {
      SPI.beginTransaction ( VS1053_SPI ) ;       // Prevent other SPI users
      digitalWrite ( cs_pin, HIGH ) ;             // Bring slave in data mode
      digitalWrite ( dcs_pin, LOW ) ;
    }

    inline void data_mode_off() const
    {
      digitalWrite ( dcs_pin, HIGH ) ;            // End data mode
      SPI.endTransaction() ;                      // Allow other SPI users
    }

    uint16_t read_register ( uint8_t _reg ) const ;
    void     write_register ( uint8_t _reg, uint16_t _value ) const ;
    void     sdi_send_buffer ( uint8_t* data, size_t len ) ;
    void     sdi_send_fillers ( size_t length ) ;
    void     wram_write ( uint16_t address, uint16_t data ) ;
    uint16_t wram_read ( uint16_t address ) ;

  public:
    // Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
    VS1053 ( uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin ) ;
    void     begin() ;                                   // Begin operation.  Sets pins correctly,
    // and prepares SPI bus.
    void     startSong() ;                               // Prepare to start playing. Call this each
    // time a new song starts.
    void     playChunk ( uint8_t* data, size_t len ) ;   // Play a chunk of data.  Copies the data to
    // the chip.  Blocks until complete.
    void     stopSong() ;                                // Finish playing a song. Call this after
    // the last playChunk call.
    void     setVolume ( uint8_t vol ) ;                 // Set the player volume.Level from 0-100,
    // higher is louder.
    void     setTone ( uint8_t* rtone ) ;                // Set the player baas/treble, 4 nibbles for
    // treble gain/freq and bass gain/freq
    uint8_t  getVolume() ;                               // Get the currenet volume setting.
    // higher is louder.
    void     printDetails ( const char *header ) ;       // Print configuration details to serial output.
    void     softReset() ;                               // Do a soft reset
    bool     testComm ( const char *header ) ;           // Test communication with module
    inline bool data_request() const
    {
      return ( digitalRead ( dreq_pin ) == HIGH ) ;
    }
} ;

//******************************************************************************************
// VS1053 class implementation.                                                            *
//******************************************************************************************

VS1053::VS1053 ( uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin ) :
  cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin)
{
}

uint16_t VS1053::read_register ( uint8_t _reg ) const
{
  uint16_t result ;

  control_mode_on() ;
  SPI.write ( 3 ) ;                                // Read operation
  SPI.write ( _reg ) ;                             // Register to write (0..0xF)
  // Note: transfer16 does not seem to work
  result = ( SPI.transfer ( 0xFF ) << 8 ) |        // Read 16 bits data
           ( SPI.transfer ( 0xFF ) ) ;
  await_data_request() ;                           // Wait for DREQ to be HIGH again
  control_mode_off() ;
  return result ;
}

void VS1053::write_register ( uint8_t _reg, uint16_t _value ) const
{
  control_mode_on( );
  SPI.write ( 2 ) ;                                // Write operation
  SPI.write ( _reg ) ;                             // Register to write (0..0xF)
  SPI.write16 ( _value ) ;                         // Send 16 bits data
  await_data_request() ;
  control_mode_off() ;
}

void VS1053::sdi_send_buffer ( uint8_t* data, size_t len )
{
  size_t chunk_length ;                            // Length of chunk 32 byte or shorter

  data_mode_on() ;
  while ( len )                                    // More to do?
  {
    await_data_request() ;                         // Wait for space available
    chunk_length = len ;
    if ( len > vs1053_chunk_size )
    {
      chunk_length = vs1053_chunk_size ;
    }
    len -= chunk_length ;
    SPI.writeBytes ( data, chunk_length ) ;
    data += chunk_length ;
  }
  data_mode_off() ;
}

void VS1053::sdi_send_fillers ( size_t len )
{
  size_t chunk_length ;                            // Length of chunk 32 byte or shorter

  data_mode_on() ;
  while ( len )                                    // More to do?
  {
    await_data_request() ;                         // Wait for space available
    chunk_length = len ;
    if ( len > vs1053_chunk_size )
    {
      chunk_length = vs1053_chunk_size ;
    }
    len -= chunk_length ;
    while ( chunk_length-- )
    {
      SPI.write ( endFillByte ) ;
    }
  }
  data_mode_off();
}

void VS1053::wram_write ( uint16_t address, uint16_t data )
{
  write_register ( SCI_WRAMADDR, address ) ;
  write_register ( SCI_WRAM, data ) ;
}

uint16_t VS1053::wram_read ( uint16_t address )
{
  write_register ( SCI_WRAMADDR, address ) ;            // Start reading from WRAM
  return read_register ( SCI_WRAM ) ;                   // Read back result
}

bool VS1053::testComm ( const char *header )
{
  // Test the communication with the VS1053 module.  The result wille be returned.
  // If DREQ is low, there is problably no VS1053 connected.  Pull the line HIGH
  // in order to prevent an endless loop waiting for this signal.  The rest of the
  // software will still work, but readbacks from VS1053 will fail.
  int       i ;                                         // Loop control
  uint16_t  r1, r2, cnt = 0 ;
  uint16_t  delta = 300 ;                               // 3 for fast SPI

  if ( !digitalRead ( dreq_pin ) )
  {
    dbgprint ( "VS1053 not properly installed!" ) ;
    // Allow testing without the VS1053 module
    pinMode ( dreq_pin,  INPUT_PULLUP ) ;               // DREQ is now input with pull-up
    return false ;                                      // Return bad result
  }
  // Further TESTING.  Check if SCI bus can write and read without errors.
  // We will use the volume setting for this.
  // Will give warnings on serial output if DEBUG is active.
  // A maximum of 20 errors will be reported.
  if ( strstr ( header, "Fast" ) )
  {
    delta = 3 ;                                         // Fast SPI, more loops
  }
  dbgprint ( header ) ;                                 // Show a header
  for ( i = 0 ; ( i < 0xFFFF ) && ( cnt < 20 ) ; i += delta )
  {
    write_register ( SCI_VOL, i ) ;                     // Write data to SCI_VOL
    r1 = read_register ( SCI_VOL ) ;                    // Read back for the first time
    r2 = read_register ( SCI_VOL ) ;                    // Read back a second time
    if  ( r1 != r2 || i != r1 || i != r2 )              // Check for 2 equal reads
    {
      dbgprint ( "VS1053 error retry SB:%04X R1:%04X R2:%04X", i, r1, r2 ) ;
      cnt++ ;
      delay ( 10 ) ;
    }
    yield() ;                                           // Allow ESP firmware to do some bookkeeping
  }
  return ( cnt == 0 ) ;                                 // Return the result
}

void VS1053::begin()
{
  pinMode      ( dreq_pin,  INPUT ) ;                   // DREQ is an input
  pinMode      ( cs_pin,    OUTPUT ) ;                  // The SCI and SDI signals
  pinMode      ( dcs_pin,   OUTPUT ) ;
  digitalWrite ( dcs_pin,   HIGH ) ;                    // Start HIGH for SCI en SDI
  digitalWrite ( cs_pin,    HIGH ) ;
  delay ( 100 ) ;
  dbgprint ( "Reset VS1053..." ) ;
  digitalWrite ( dcs_pin,   LOW ) ;                     // Low & Low will bring reset pin low
  digitalWrite ( cs_pin,    LOW ) ;
  delay ( 2000 ) ;
  dbgprint ( "End reset VS1053..." ) ;
  digitalWrite ( dcs_pin,   HIGH ) ;                    // Back to normal again
  digitalWrite ( cs_pin,    HIGH ) ;
  delay ( 500 ) ;
  // Init SPI in slow mode ( 0.2 MHz )
  VS1053_SPI = SPISettings ( 200000, MSBFIRST, SPI_MODE0 ) ;
  //printDetails ( "Right after reset/startup" ) ;
  delay ( 20 ) ;
  //printDetails ( "20 msec after reset" ) ;
  testComm ( "Slow SPI,Testing VS1053 read/write registers..." ) ;
  // Most VS1053 modules will start up in midi mode.  The result is that there is no audio
  // when playing MP3.  You can modify the board, but there is a more elegant way:
  wram_write ( 0xC017, 3 ) ;                            // GPIO DDR = 3
  wram_write ( 0xC019, 0 ) ;                            // GPIO ODATA = 0
  delay ( 100 ) ;
  //printDetails ( "After test loop" ) ;
  softReset() ;                                         // Do a soft reset
  // Switch on the analog parts
  write_register ( SCI_AUDATA, 44100 + 1 ) ;            // 44.1kHz + stereo
  // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
  write_register ( SCI_CLOCKF, 6 << 12 ) ;              // Normal clock settings multiplyer 3.0 = 12.2 MHz
  //SPI Clock to 4 MHz. Now you can set high speed SPI clock.
  VS1053_SPI = SPISettings ( 4000000, MSBFIRST, SPI_MODE0 ) ;
  write_register ( SCI_MODE, _BV ( SM_SDINEW ) | _BV ( SM_LINE1 ) ) ;
  testComm ( "Fast SPI, Testing VS1053 read/write registers again..." ) ;
  delay ( 10 ) ;
  await_data_request() ;
  endFillByte = wram_read ( 0x1E06 ) & 0xFF ;
  dbgprint ( "endFillByte is %X", endFillByte ) ;
  //printDetails ( "After last clocksetting" ) ;
  delay ( 100 ) ;
}

void VS1053::setVolume ( uint8_t vol )
{
  // Set volume.  Both left and right.
  // Input value is 0..100.  100 is the loudest.
  // Clicking reduced by using 0xf8 to 0x00 as limits.
  uint16_t value ;                                      // Value to send to SCI_VOL

  if ( vol != curvol )
  {
    curvol = vol ;                                      // Save for later use
    value = map ( vol, 0, 100, 0xF8, 0x00 ) ;           // 0..100% to one channel
    value = ( value << 8 ) | value ;
    write_register ( SCI_VOL, value ) ;                 // Volume left and right
  }
}

void VS1053::setTone ( uint8_t *rtone )                 // Set bass/treble (4 nibbles)
{
  // Set tone characteristics.  See documentation for the 4 nibbles.
  uint16_t value = 0 ;                                  // Value to send to SCI_BASS
  int      i ;                                          // Loop control

  for ( i = 0 ; i < 4 ; i++ )
  {
    value = ( value << 4 ) | rtone[i] ;                 // Shift next nibble in
  }
  write_register ( SCI_BASS, value ) ;                  // Volume left and right
}

uint8_t VS1053::getVolume()                             // Get the currenet volume setting.
{
  return curvol ;
}

void VS1053::startSong()
{
  sdi_send_fillers ( 10 ) ;
}

void VS1053::playChunk ( uint8_t* data, size_t len )
{
  sdi_send_buffer ( data, len ) ;
}

void VS1053::stopSong()
{
  uint16_t modereg ;                     // Read from mode register
  int      i ;                           // Loop control

  sdi_send_fillers ( 2052 ) ;
  delay ( 10 ) ;
  write_register ( SCI_MODE, _BV ( SM_SDINEW ) | _BV ( SM_CANCEL ) ) ;
  for ( i = 0 ; i < 200 ; i++ )
  {
    sdi_send_fillers ( 32 ) ;
    modereg = read_register ( SCI_MODE ) ;  // Read status
    if ( ( modereg & _BV ( SM_CANCEL ) ) == 0 )
    {
      sdi_send_fillers ( 2052 ) ;
      dbgprint ( "Song stopped correctly after %d msec", i * 10 ) ;
      return ;
    }
    delay ( 10 ) ;
  }
  printDetails ( "Song stopped incorrectly!" ) ;
}

void VS1053::softReset()
{
  write_register ( SCI_MODE, _BV ( SM_SDINEW ) | _BV ( SM_RESET ) ) ;
  delay ( 10 ) ;
  await_data_request() ;
}

void VS1053::printDetails ( const char *header )
{
  uint16_t     regbuf[16] ;
  uint8_t      i ;

  dbgprint ( header ) ;
  dbgprint ( "REG   Contents" ) ;
  dbgprint ( "---   -----" ) ;
  for ( i = 0 ; i <= SCI_num_registers ; i++ )
  {
    regbuf[i] = read_register ( i ) ;
  }
  for ( i = 0 ; i <= SCI_num_registers ; i++ )
  {
    delay ( 5 ) ;
    dbgprint ( "%3X - %5X", i, regbuf[i] ) ;
  }
}

// The object for the MP3 player
VS1053 vs1053player (  VS1053_CS, VS1053_DCS, VS1053_DREQ ) ;

//******************************************************************************************
// End VS1053 stuff.                                                                       *
//******************************************************************************************



//******************************************************************************************
// Ringbuffer (fifo) routines.                                                             *
//******************************************************************************************
//******************************************************************************************
//                              R I N G S P A C E                                          *
//******************************************************************************************
inline bool ringspace()
{
  return ( rcount < RINGBFSIZ ) ;     // True is at least one byte of free space is available
}


//******************************************************************************************
//                              R I N G A V A I L                                          *
//******************************************************************************************
inline uint16_t ringavail()
{
  return rcount ;                     // Return number of bytes available
}


//******************************************************************************************
//                                P U T R I N G                                            *
//******************************************************************************************
void putring ( uint8_t b )                 // Put one byte in the ringbuffer
{
  // No check on available space.  See ringspace()
  *(ringbuf + rbwindex) = b ;         // Put byte in ringbuffer
  if ( ++rbwindex == RINGBFSIZ )      // Increment pointer and
  {
    rbwindex = 0 ;                    // wrap at end
  }
  rcount++ ;                          // Count number of bytes in the
}


//******************************************************************************************
//                                G E T R I N G                                            *
//******************************************************************************************
uint8_t getring()
{
  // Assume there is always something in the bufferpace.  See ringavail()
  if ( ++rbrindex == RINGBFSIZ )      // Increment pointer and
  {
    rbrindex = 0 ;                    // wrap at end
  }
  rcount-- ;                          // Count is now one less
  return *(ringbuf + rbrindex) ;      // return the oldest byte
}

//******************************************************************************************
//                               E M P T Y R I N G                                         *
//******************************************************************************************
void emptyring()
{
  rbwindex = 0 ;                      // Reset ringbuffer administration
  rbrindex = RINGBFSIZ - 1 ;
  rcount = 0 ;
}


//******************************************************************************************
//                              U T F 8 A S C I I                                          *
//******************************************************************************************
// UTF8-Decoder: convert UTF8-string to extended ASCII.                                    *
// Convert a single Character from UTF8 to Extended ASCII.                                 *
// Return "0" if a byte has to be ignored.                                                 *
//******************************************************************************************
byte utf8ascii ( byte ascii )
{
  static const byte lut_C3[] = 
         { "AAAAAAACEEEEIIIIDNOOOOO#0UUUU###aaaaaaaceeeeiiiidnooooo##uuuuyyy" } ;
  static byte       c1 ;              // Last character buffer
  byte              res = 0 ;         // Result, default 0

  if ( ascii <= 0x7F )                // Standard ASCII-set 0..0x7F handling
  {
    c1 = 0 ;
    res = ascii ;                     // Return unmodified
  }
  else
  {
    switch ( c1 )                     // Conversion depending on first UTF8-character
    {   
      case 0xC2: res = '~' ;
                 break ;
      case 0xC3: res = lut_C3[ascii-128] ;
                 break ;
      case 0x82: if ( ascii == 0xAC )
                 {
                    res = 'E' ;       // Special case Euro-symbol
                 }
    }
    c1 = ascii ;                      // Remember actual character
  }
  return res ;                        // Otherwise: return zero, if character has to be ignored
}


//******************************************************************************************
//                              U T F 8 A S C I I                                          *
//******************************************************************************************
// In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!).                  *
//******************************************************************************************
void utf8ascii ( char* s )
{
  int  i, k = 0 ;                     // Indexes for in en out string
  char c ;

  for ( i = 0 ; s[i] ; i++ )          // For every input character
  {
    c = utf8ascii ( s[i] ) ;          // Translate if necessary
    if ( c )                          // Good translation?
    {
      s[k++] = c ;                    // Yes, put in output string
    }
  }
  s[k] = 0 ;                          // Take care of delimeter
}


//******************************************************************************************
//                                  D B G P R I N T                                        *
//******************************************************************************************
// Send a line of info to serial output.  Works like vsprintf(), but checks the BEDUg flag.*
// Print only if DEBUG flag is true.  Always returns the the formatted string.             *
//******************************************************************************************
char* dbgprint ( const char* format, ... )
{
  static char sbuf[DEBUG_BUFFER_SIZE] ;                // For debug lines
  va_list varArgs ;                                    // For variable number of params

  va_start ( varArgs, format ) ;                       // Prepare parameters
  vsnprintf ( sbuf, sizeof(sbuf), format, varArgs ) ;  // Format the message
  va_end ( varArgs ) ;                                 // End of using parameters
  if ( DEBUG )                                         // DEBUG on?
  {
    Serial.print ( "D: " ) ;                           // Yes, print prefix
    Serial.println ( sbuf ) ;                          // and the info
  }
  return sbuf ;                                        // Return stored string
}


//******************************************************************************************
//                             G E T E N C R Y P T I O N T Y P E                           *
//******************************************************************************************
// Read the encryption type of the network and return as a 4 byte name                     *
//*********************4********************************************************************
const char* getEncryptionType ( int thisType )
{
  switch (thisType)
  {
    case ENC_TYPE_WEP:
      return "WEP " ;
    case ENC_TYPE_TKIP:
      return "WPA " ;
    case ENC_TYPE_CCMP:
      return "WPA2" ;
    case ENC_TYPE_NONE:
      return "None" ;
    case ENC_TYPE_AUTO:
      return "Auto" ;
  }
  return "????" ;
}


//******************************************************************************************
//                                L I S T N E T W O R K S                                  *
//******************************************************************************************
// List the available networks and select the strongest.                                   *
// Acceptable networks are those who have a "SSID.pw" file in the SPIFFS.                  *
// SSIDs of available networks will be saved for use in webinterface.                      *
//******************************************************************************************
void listNetworks()
{
  int         maxsig = -1000 ;   // Used for searching strongest WiFi signal
  int         newstrength ;
  byte        encryption ;       // TKIP(WPA)=2, WEP=5, CCMP(WPA)=4, NONE=7, AUTO=8
  const char* acceptable ;       // Netwerk is acceptable for connection
  int         i ;                // Loop control
  String      sassid ;           // Search string in anetworks

  ini_block.ssid = String ( "none" ) ;                   // No selceted network yet
  // scan for nearby networks:
  dbgprint ( "* Scan Networks *" ) ;
  int numSsid = WiFi.scanNetworks() ;
  if ( numSsid == -1 )
  {
    dbgprint ( "Couldn't get a wifi connection" ) ;
    return ;
  }
  // print the list of networks seen:
  dbgprint ( "Number of available networks: %d",
             numSsid ) ;
  // Print the network number and name for each network found and
  // find the strongest acceptable network
  for ( i = 0 ; i < numSsid ; i++ )
  {
    acceptable = "" ;                                    // Assume not acceptable
    newstrength = WiFi.RSSI ( i ) ;                      // Get the signal strenght
    sassid = WiFi.SSID ( i ) + String ( "|" ) ;          // For search string
    if ( anetworks.indexOf ( sassid ) >= 0 )             // Is this SSID acceptable?
    {
      acceptable = "Acceptable" ;
      if ( newstrength > maxsig )                        // This is a better Wifi
      {
        maxsig = newstrength ;
        ini_block.ssid = WiFi.SSID ( i ) ;               // Remember SSID name
      }
    }
    encryption = WiFi.encryptionType ( i ) ;
    dbgprint ( "%2d - %-25s Signal: %3d dBm Encryption %4s  %s",
               i + 1, WiFi.SSID ( i ).c_str(), WiFi.RSSI ( i ),
               getEncryptionType ( encryption ),
               acceptable ) ;
    // Remember this network for later use
    networks += WiFi.SSID ( i ) + String ( "|" ) ;
  }
  dbgprint ( "--------------------------------------" ) ;
}


//******************************************************************************************
//                                  T I M E R 1 0 S E C                                    *
//******************************************************************************************
// Extra watchdog.  Called every 10 seconds.                                               *
// If totalcount has not been changed, there is a problem and playing will stop.           *
// Note that a "yield()" within this routine or in called functions will cause a crash!    *
//******************************************************************************************
void timer10sec()
{
  static uint32_t oldtotalcount = 7321 ;          // Needed foor change detection
  static uint8_t  morethanonce = 0 ;              // Counter for succesive fails
  static uint8_t  t600 = 0 ;                      // Counter for 10 minutes

  if ( datamode & ( INIT | HEADER | DATA |        // Test op playing
                    METADATA | PLAYLISTINIT |
                    PLAYLISTHEADER |
                    PLAYLISTDATA ) )
  {
    if ( totalcount == oldtotalcount )            // Still playing?
    {
      dbgprint ( "No data input" ) ;              // No data detected!
      if ( morethanonce > 10 )                    // Happened too many times?
      {
        dbgprint ( "Going to restart..." ) ;
        ESP.restart() ;                           // Reset the CPU, probably no return
      }
      if ( datamode & ( PLAYLISTDATA |            // In playlist mode?
                        PLAYLISTINIT |
                        PLAYLISTHEADER ) )
      {
        playlist_num = 0 ;                        // Yes, end of playlist
      }
      if ( ( morethanonce > 0 ) ||                // Happened more than once?
           ( playlist_num > 0 ) )                 // Or playlist active?
      {
        datamode = STOPREQD ;                     // Stop player
        ini_block.newpreset++ ;                   // Yes, try next channel
        dbgprint ( "Trying other station/file..." ) ;
      }
      morethanonce++ ;                            // Count the fails
    }
    else
    {
      if ( morethanonce )                         // Recovered from data loss?
      {
        dbgprint ( "Recovered from dataloss" ) ;
        morethanonce = 0 ;                        // Data see, reset failcounter
      }
      oldtotalcount = totalcount ;                // Save for comparison in next cycle
    }
    if ( t600++ == 60 )                           // 10 minutes over?
    {
      t600 = 0 ;                                  // Yes, reset counter
      dbgprint ( "10 minutes over" ) ;
      publishIP() ;                               // Re-publish IP
    }
  }
}


//******************************************************************************************
//                                  A N A G E T S W                                        *
//******************************************************************************************
// Translate analog input to switch number.  0 is inactive.                                *
// Note that it is adviced to avoid expressions as the argument for the abs function.      *
//******************************************************************************************
uint8_t anagetsw ( uint16_t v )
{
  int      i ;                                    // Loop control
  int      oldmindist = 1000 ;                    // Detection least difference
  int      newdist ;                              // New found difference
  uint8_t  sw = 0 ;                               // Number of switch detected (0 or 1..3)

  if ( v > analogrest )                           // Inactive level?
  {
    for ( i = 0 ; i < NUMANA ; i++ )
    {
      newdist = analogsw[i] - v ;                  // Compute difference
      newdist = abs ( newdist ) ;                  // Make it absolute
      if ( newdist < oldmindist )                  // New least difference?
      {
        oldmindist = newdist ;                     // Yes, remember
        sw = i + 1 ;                               // Remember switch
      }
    }
  }
  return sw ;                                      // Return active switch
}


//******************************************************************************************
//                               T E S T F I L E                                           *
//******************************************************************************************
// Test the performance of SPIFFS read.                                                    *
//******************************************************************************************
void testfile ( String fspec )
{
  String   path ;                                      // Full file spec
  File     tfile ;                                     // File containing mp3
  uint32_t len, savlen ;                               // File length
  uint32_t t0, t1, told ;                              // For time test
  uint32_t t_error = 0 ;                               // Number of slow reads

  dbgprint ( "Start test of file %s", fspec.c_str() ) ;
  t0 = millis() ;                                      // Timestamp at start
  t1 = t0 ;                                            // Prevent uninitialized value
  told = t0 ;                                          // For report
  path = String ( "/" ) + fspec ;                      // Form full path
  tfile = SPIFFS.open ( path, "r" ) ;                  // Open the file
  if ( tfile )
  {
    len = tfile.available() ;                          // Get file length
    savlen = len ;                                     // Save for result print
    while ( len-- )                                    // Any data left?
    {
      t1 = millis() ;                                  // To meassure read time
      tfile.read() ;                                   // Read one byte
      if ( ( millis() - t1 ) > 5 )                     // Read took more than 5 msec?
      {
        t_error++ ;                                    // Yes, count slow reads
      }
      if ( ( len % 100 ) == 0 )                        // Yield reguarly
      {
        yield() ;
      }
      if ( ( ( t1 - told ) / 1000 ) > 0 || len == 0 )
      {
        // Show results for debug
        dbgprint ( "Read %s, length %d/%d took %d seconds, %d slow reads",
                   fspec.c_str(), savlen - len, savlen, ( t1 - t0 ) / 1000, t_error ) ;
        told = t1 ;
      }
      if ( ( t1 - t0 ) > 100000 )                      // Give up after 100 seconds
      {
        dbgprint ( "Give up..." ) ;
        break ;
      }
    }
    tfile.close() ;
    dbgprint ( "EOF" ) ;                               // End of file
  }
}


//******************************************************************************************
//                                  T I M E R 1 0 0                                        *
//******************************************************************************************
// Examine button every 100 msec.                                                          *
//******************************************************************************************
void timer100()
{
  static int     count10sec = 0 ;                 // Counter for activatie 10 seconds process
  static int     oldval2 = HIGH ;                 // Previous value of digital input button 2
#if ( not ( defined ( USETFT ) ) )
  static int     oldval1 = HIGH ;                 // Previous value of digital input button 1
  static int     oldval3 = HIGH ;                 // Previous value of digital input button 3
#endif
  int            newval ;                         // New value of digital input switch
  uint16_t       v ;                              // Analog input value 0..1023
  static uint8_t aoldval = 0 ;                    // Previous value of analog input switch
  uint8_t        anewval ;                        // New value of analog input switch (0..3)

  if ( ++count10sec == 100  )                     // 10 seconds passed?
  {
    timer10sec() ;                                // Yes, do 10 second procedure
    count10sec = 0 ;                              // Reset count
  }
  else
  {
    newval = digitalRead ( BUTTON2 ) ;            // Test if below certain level
    if ( newval != oldval2 )                      // Change?
    {
      oldval2 = newval ;                          // Yes, remember value
      if ( newval == LOW )                        // Button pushed?
      {
        ini_block.newpreset = currentpreset + 1 ; // Yes, goto next preset station
        //dbgprint ( "Digital button 2 pushed" ) ;
      }
      return ;
    }
#if ( not ( defined ( USETFT ) ) )
    newval = digitalRead ( BUTTON1 ) ;            // Test if below certain level
    if ( newval != oldval1 )                      // Change?
    {
      oldval1 = newval ;                          // Yes, remember value
      if ( newval == LOW )                        // Button pushed?
      {
        ini_block.newpreset = 0 ;                 // Yes, goto first preset station
        //dbgprint ( "Digital button 1 pushed" ) ;
      }
      return ;
    }
    // Note that BUTTON3 has inverted input
    newval = digitalRead ( BUTTON3 ) ;            // Test if below certain level
    newval = HIGH + LOW - newval ;                // Reverse polarity
    if ( newval != oldval3 )                      // Change?
    {
      oldval3 = newval ;                          // Yes, remember value
      if ( newval == LOW )                        // Button pushed?
      {
        ini_block.newpreset = currentpreset - 1 ; // Yes, goto previous preset station
        //dbgprint ( "Digital button 3 pushed" ) ;
      }
      return ;
    }
#endif
    v = analogRead ( A0 ) ;                       // Read analog value
    anewval = anagetsw ( v ) ;                    // Check analog value for program switches
    if ( anewval != aoldval )                     // Change?
    {
      aoldval = anewval ;                         // Remember value for change detection
      if ( anewval != 0 )                         // Button pushed?
      {
        //dbgprint ( "Analog button %d pushed, v = %d", anewval, v ) ;
        if ( anewval == 1 )                       // Button 1?
        {
          ini_block.newpreset = 0 ;               // Yes, goto first preset
        }
        else if ( anewval == 2 )                  // Button 2?
        {
          ini_block.newpreset = currentpreset + 1 ; // Yes, goto next preset
        }
        else if ( anewval == 3 )                  // Button 3?
        {
          ini_block.newpreset = currentpreset - 1 ; // Yes, goto previous preset
        }
      }
    }
  }
}


//******************************************************************************************
//                              D I S P L A Y V O L U M E                                  *
//******************************************************************************************
// Show the current volume as an indicator on the screen.                                  *
//******************************************************************************************
void displayvolume()
{
#if defined ( USETFT )
  static uint8_t oldvol = 0 ;                        // Previous volume
  uint8_t        pos ;                               // Positon of volume indicator

  if ( vs1053player.getVolume() != oldvol )
  {
    pos = map ( vs1053player.getVolume(), 0, 100, 0, 160 ) ;
  }
  tft.fillRect ( 0, 126, pos, 2, RED ) ;             // Paint red part
  tft.fillRect ( pos, 126, 160 - pos, 2, GREEN ) ;   // Paint green part
#endif
}


//******************************************************************************************
//                              D I S P L A Y I N F O                                      *
//******************************************************************************************
// Show a string on the LCD at a specified y-position in a specified color                 *
//******************************************************************************************
#if defined ( USETFT )
void displayinfo ( const char* str, uint16_t pos, uint16_t height, uint16_t color )
{
  char buf [ strlen ( str ) + 1 ] ;             // Need some buffer space
  
  strcpy ( buf, str ) ;                         // Make a local copy of the string
  utf8ascii ( buf ) ;                           // Convert possible UTF8
  tft.fillRect ( 0, pos, 160, height, BLACK ) ; // Clear the space for new info
  tft.setTextColor ( color ) ;                  // Set the requested color
  tft.setCursor ( 0, pos ) ;                    // Prepare to show the info
  tft.println ( buf ) ;                         // Show the string
}
#else
#define displayinfo(a,b,c,d)                    // Empty declaration
#endif


//******************************************************************************************
//                        S H O W S T R E A M T I T L E                                    *
//******************************************************************************************
// Show artist and songtitle if present in metadata.                                       *
// Show always if full=true.                                                               *
//******************************************************************************************
void showstreamtitle ( const char *ml, bool full )
{
  char*             p1 ;
  char*             p2 ;
  char              streamtitle[150] ;           // Streamtitle from metadata

  if ( strstr ( ml, "StreamTitle=" ) )
  {
    dbgprint ( "Streamtitle found, %d bytes", strlen ( ml ) ) ;
    dbgprint ( ml ) ;
    p1 = (char*)ml + 12 ;                       // Begin of artist and title
    if ( ( p2 = strstr ( ml, ";" ) ) )          // Search for end of title
    {
      if ( *p1 == '\'' )                        // Surrounded by quotes?
      {
        p1++ ;
        p2-- ;
      }
      *p2 = '\0' ;                              // Strip the rest of the line
    }
    // Save last part of string as streamtitle.  Protect against buffer overflow
    strncpy ( streamtitle, p1, sizeof ( streamtitle ) ) ;
    streamtitle[sizeof ( streamtitle ) - 1] = '\0' ;
  }
  else if ( full )
  {
    // Info probably from playlist
    strncpy ( streamtitle, ml, sizeof ( streamtitle ) ) ;
    streamtitle[sizeof ( streamtitle ) - 1] = '\0' ;
  }
  else
  {
    icystreamtitle = "" ;                       // Unknown type
    return ;                                    // Do not show
  }
  // Save for status request from browser ;
  icystreamtitle = streamtitle ;
  if ( ( p1 = strstr ( streamtitle, " - " ) ) ) // look for artist/title separator
  {
    *p1++ = '\n' ;                              // Found: replace 3 characters by newline
    p2 = p1 + 2 ;
    if ( *p2 == ' ' )                           // Leading space in title?
    {
      p2++ ;
    }
    strcpy ( p1, p2 ) ;                         // Shift 2nd part of title 2 or 3 places
  }
  displayinfo ( streamtitle, 20, 40, CYAN ) ;   // Show title at position 20
}


//******************************************************************************************
//                            S T O P _ M P 3 C L I E N T                                  *
//******************************************************************************************
// Disconnect from the server.                                                             *
//******************************************************************************************
void stop_mp3client ()
{
  if ( mp3client )
  {
    if ( mp3client->connected() )                    // Need to stop client?
    {
      dbgprint ( "Stopping client" ) ;               // Stop connection to host
      mp3client->flush() ;
      mp3client->stop() ;
      delay ( 500 ) ;
    }
    delete ( mp3client ) ;
    mp3client = NULL ;
  }
}


//******************************************************************************************
//                            C O N N E C T T O H O S T                                    *

//******************************************************************************************
// Connect to the Internet radio server specified by newpreset.                            *
//******************************************************************************************
bool connecttohost()
{
  int         inx ;                                 // Position of ":" in hostname
  char*       pfs ;                                 // Pointer to formatted string
  int         port = 80 ;                           // Port number for host
  String      extension = "/" ;                     // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
  String      hostwoext ;                           // Host without extension and portnumber

  stop_mp3client() ;                                // Disconnect if still connected
  dbgprint ( "Connect to new host %s", host.c_str() ) ;
  displayinfo ( "   ** Internet radio **", 0, 20, WHITE ) ;
  datamode = INIT ;                                 // Start default in metamode
  chunked = false ;                                 // Assume not chunked
  if ( host.endsWith ( ".m3u" ) )                   // Is it an m3u playlist?
  {
    playlist = host ;                               // Save copy of playlist URL
    datamode = PLAYLISTINIT ;                       // Yes, start in PLAYLIST mode
    if ( playlist_num == 0 )                        // First entry to play?
    {
      playlist_num = 1 ;                            // Yes, set index
    }
    dbgprint ( "Playlist request, entry %d", playlist_num ) ;
  }
  // In the URL there may be an extension
  inx = host.indexOf ( "/" ) ;                      // Search for begin of extension
  if ( inx > 0 )                                    // Is there an extension?
  {
    extension = host.substring ( inx ) ;            // Yes, change the default
    hostwoext = host.substring ( 0, inx ) ;         // Host without extension
  }
  // In the URL there may be a portnumber
  inx = host.indexOf ( ":" ) ;                      // Search for separator
  if ( inx >= 0 )                                   // Portnumber available?
  {
    port = host.substring ( inx + 1 ).toInt() ;     // Get portnumber as integer
    hostwoext = host.substring ( 0, inx ) ;         // Host without portnumber
  }
  pfs = dbgprint ( "Connect to %s on port %d, extension %s",
                   hostwoext.c_str(), port, extension.c_str() ) ;
  displayinfo ( pfs, 60, 66, YELLOW ) ;             // Show info at position 60..125
  mp3client = new WiFiClient() ;
  if ( mp3client->connect ( hostwoext.c_str(), port ) )
  {
    dbgprint ( "Connected to server" ) ;
    // This will send the request to the server. Request metadata.
    mp3client->print ( String ( "GET " ) +
                       extension +
                      String ( " HTTP/1.1\r\n" ) +
                      String ( "Host: " ) +
                      hostwoext +
                      String ( "\r\n" ) +
                      String ( "Icy-MetaData:1\r\n" ) +
                      String ( "Connection: close\r\n\r\n" ) ) ;
    return true ;
  }
  dbgprint ( "Request %s failed!", host.c_str() ) ;
  return false ;
}


//******************************************************************************************
//                               C O N N E C T T O F I L E                                 *
//******************************************************************************************
// Open the local mp3-file.                                                                *
//******************************************************************************************
bool connecttofile()
{
  String path ;                                           // Full file spec
  char*  p ;                                              // Pointer to filename

  displayinfo ( "   **** MP3 Player ****", 0, 20, WHITE ) ;
  path = host.substring ( 9 ) ;                           // Path, skip the "localhost" part
  mp3file = SPIFFS.open ( path, "r" ) ;                   // Open the file
  if ( !mp3file )
  {
    dbgprint ( "Error opening file %s", path.c_str() ) ;  // No luck
    return false ;
  }
  p = (char*)path.c_str() + 1 ;                           // Point to filename
  showstreamtitle ( p, true ) ;                           // Show the filename as title
  displayinfo ( "Playing from local file",
                60, 68, YELLOW ) ;                        // Show Source at position 60
  icyname = "" ;                                          // No icy name yet
  chunked = false ;                                       // File not chunked
  return true ;
}


//******************************************************************************************
//                               C O N N E C T W I F I                                     *
//******************************************************************************************
// Connect to WiFi using passwords available in the SPIFFS.                                *
// If connection fails, an AP is created and the function returns false.                   *
//******************************************************************************************
bool connectwifi()
{
  char*  pfs ;                                         // Pointer to formatted string

  WiFi.disconnect() ;                                  // After restart the router could
  WiFi.softAPdisconnect(true) ;                        // still keep the old connection
  WiFi.begin ( ini_block.ssid.c_str(),
               ini_block.passwd.c_str() ) ;            // Connect to selected SSID
  dbgprint ( "Try WiFi %s", ini_block.ssid.c_str() ) ; // Message to show during WiFi connect
  if (  WiFi.waitForConnectResult() != WL_CONNECTED )  // Try to connect
  {
    dbgprint ( "WiFi Failed!  Trying to setup AP with name %s and password %s.", NAME, NAME ) ;
    WiFi.softAP ( NAME, NAME ) ;                       // This ESP will be an AP
    delay ( 5000 ) ;
    pfs = dbgprint ( "IP = 192.168.4.1" ) ;            // Address if AP
    return false ;
  }
  pfs = dbgprint ( "IP = %d.%d.%d.%d",
                   WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] ) ;
#if defined ( USETFT )
  tft.println ( pfs ) ;
#endif
  return true ;
}


//******************************************************************************************
//                                   O T A S T A R T                                       *
//******************************************************************************************
// Update via WiFi has been started by Arduino IDE.                                        *
//******************************************************************************************
void otastart()
{
  dbgprint ( "OTA Started" ) ;
}


//******************************************************************************************
//                          R E A D H O S T F R O M I N I F I L E                          *
//******************************************************************************************
// Read the mp3 host from the ini-file specified by the parameter.                         *
// The host will be returned.                                                              *
//******************************************************************************************
String readhostfrominifile ( int8_t preset )
{
  String      path ;                                   // Full file spec as string
  File        inifile ;                                // File containing URL with mp3
  char        tkey[10] ;                               // Key as an array of chars
  String      line ;                                   // Input line from .ini file
  String      linelc ;                                 // Same, but lowercase
  int         inx ;                                    // Position within string
  String      res = "" ;                               // Assume not found

  path = String ( INIFILENAME ) ;                      // Form full path
  inifile = SPIFFS.open ( path, "r" ) ;                // Open the file
  if ( inifile )
  {
    sprintf ( tkey, "preset_%02d", preset ) ;           // Form the search key
    while ( inifile.available() )
    {
      line = inifile.readStringUntil ( '\n' ) ;        // Read next line
      linelc = line ;                                  // Copy for lowercase
      linelc.toLowerCase() ;                           // Set to lowercase
      if ( linelc.startsWith ( tkey ) )                // Found the key?
      {
        inx = line.indexOf ( "=" ) ;                   // Get position of "="
        if ( inx > 0 )                                 // Equal sign present?
        {
          line.remove ( 0, inx + 1 ) ;                 // Yes, remove key
          res = chomp ( line ) ;                       // Remove garbage
          break ;                                      // End the while loop
        }
      }
    }
    inifile.close() ;                                  // Close the file
  }
  else
  {
    dbgprint ( "File %s not found, please create one!", INIFILENAME ) ;
  }
  return res ;
}


//******************************************************************************************
//                               R E A D I N I F I L E                                     *
//******************************************************************************************
// Read the .ini file and interpret the commands.                                          *
//******************************************************************************************
void readinifile()
{
  String      path ;                                   // Full file spec as string
  File        inifile ;                                // File containing URL with mp3
  String      line ;                                   // Input line from .ini file

  path = String ( INIFILENAME ) ;                      // Form full path
  inifile = SPIFFS.open ( path, "r" ) ;                // Open the file
  if ( inifile )
  {
    while ( inifile.available() )
    {
      line = inifile.readStringUntil ( '\n' ) ;        // Read next line
      analyzeCmd ( line.c_str() ) ;
    }
    inifile.close() ;                                  // Close the file
  }
  else
  {
    dbgprint ( "File %s not found, use save command to create one!", INIFILENAME ) ;
  }
}


//******************************************************************************************
//                            P U B L I S H I P                                            *
//******************************************************************************************
// Publish IP to MQTT broker.                                                              *
//******************************************************************************************
void publishIP()
{
  IPAddress ip ;
  char      ipstr[20] ;                          // Hold IP as string

  if ( ini_block.mqttpubtopic.length() )        // Topic to publish?
  {
    ip = WiFi.localIP() ;
    // Publish IP-adress.  qos=1, retain=true
    sprintf ( ipstr, "%d.%d.%d.%d",
              ip[0], ip[1], ip[2], ip[3] ) ;
    mqttclient.publish ( ini_block.mqttpubtopic.c_str(), 1, true, ipstr ) ;
    dbgprint ( "Publishing IP %s to topic %s",
               ipstr, ini_block.mqttpubtopic.c_str() ) ;
  }
}


//******************************************************************************************
//                            O N M Q T T C O N N E C T                                    *
//******************************************************************************************
// Will be called on connection to the broker.  Subscribe to our topic and publish a topic.*
//******************************************************************************************
void onMqttConnect( bool sessionPresent )
{
  uint16_t    packetIdSub ;
  const char* present = "is" ;                      // Assume Session is present

  if ( !sessionPresent )
  {
    present = "is not" ;                            // Session is NOT present
  }
  dbgprint ( "MQTT Connected to the broker %s, session %s present",
             ini_block.mqttbroker.c_str(), present ) ;
  packetIdSub = mqttclient.subscribe ( ini_block.mqtttopic.c_str(), 2 ) ;
  dbgprint ( "Subscribing to %s at QoS 2, packetId = %d ",
             ini_block.mqtttopic.c_str(),
             packetIdSub ) ;
  publishIP() ;                                     // Topic to publish: IP
}


//******************************************************************************************
//                      O N M Q T T D I S C O N N E C T                                    *
//******************************************************************************************
// Will be called on disconnect.                                                           *
//******************************************************************************************
void onMqttDisconnect ( AsyncMqttClientDisconnectReason reason )
{
  dbgprint ( "MQTT Disconnected from the broker, reason %d, reconnecting...",
             reason ) ;
  if ( mqttcount < MAXMQTTCONNECTS )            // Try again?
  {
    mqttcount++ ;                               // Yes, count number of tries
    mqttclient.connect() ;                      // Reconnect
  }
}


//******************************************************************************************
//                      O N M Q T T S U B S C R I B E                                      *
//******************************************************************************************
// Will be called after a successful subscribe.                                            *
//******************************************************************************************
void onMqttSubscribe ( uint16_t packetId, uint8_t qos )
{
  dbgprint ( "MQTT Subscribe acknowledged, packetId = %d, QoS = %d",
             packetId, qos ) ;
}


//******************************************************************************************
//                              O N M Q T T U N S U B S C R I B E                          *
//******************************************************************************************
// Will be executed if this program unsubscribes from a topic.                             *
// Not used at the moment.                                                                 *
//******************************************************************************************
void onMqttUnsubscribe ( uint16_t packetId )
{
  dbgprint ( "MQTT Unsubscribe acknowledged, packetId = %d",
             packetId ) ;
}


//******************************************************************************************
//                            O N M Q T T M E S S A G E                                    *
//******************************************************************************************
// Executed when a subscribed message is received.                                         *
// Note that message is not delimited by a '\0'.                                           *
//******************************************************************************************
void onMqttMessage ( char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                     size_t len, size_t index, size_t total )
{
  char*  reply ;                                    // Result from analyzeCmd

  // Available properties.qos, properties.dup, properties.retain
  if ( len >= sizeof(cmd) )                         // Message may not be too long
  {
    len = sizeof(cmd) - 1 ;
  }
  strncpy ( cmd, payload, len ) ;                   // Make copy of message
  cmd[len] = '\0' ;                                 // Take care of delimeter
  dbgprint ( "MQTT message arrived [%s], lenght = %d, %s", topic, len, cmd ) ;
  reply = analyzeCmd ( cmd ) ;                      // Analyze command and handle it
  dbgprint ( reply ) ;                              // Result for debugging
}


//******************************************************************************************
//                             O N M Q T T P U B L I S H                                   *
//******************************************************************************************
// Will be executed if a message is published by this program.                             *
// Not used at the moment.                                                                 *
//******************************************************************************************
void onMqttPublish ( uint16_t packetId )
{
  dbgprint ( "MQTT Publish acknowledged, packetId = %d",
             packetId ) ;
}


//******************************************************************************************
//                             S C A N S E R I A L                                         *
//******************************************************************************************
// Listen to commands on the Serial inputline.                                             *
//******************************************************************************************
void scanserial()
{
  static String serialcmd ;                      // Command from Serial input
  char          c ;                              // Input character
  char*         reply ;                          // Reply string froma analyzeCmd
  uint16_t      len ;                            // Length of input string

  while ( Serial.available() )                   // Any input seen?
  {
    c =  (char)Serial.read() ;                   // Yes, read the next input character
    //Serial.write ( c ) ;                       // Echo
    len = serialcmd.length() ;                   // Get the length of the current string
    if ( ( c == '\n' ) || ( c == '\r' ) )
    {
      if ( len )
      {
        strncpy ( cmd, serialcmd.c_str(), sizeof(cmd) ) ;
        reply = analyzeCmd ( cmd) ;              // Analyze command and handle it
        dbgprint ( reply ) ;                     // Result for debugging
        serialcmd = "" ;                         // Prepare for new command
      }
    }
    if ( c >= ' ' )                              // Only accept useful characters
    {
      serialcmd += c ;                           // Add to the command
    }
    if ( len >= ( sizeof(cmd) - 2 )  )           // Check for excessive length
    {
      serialcmd = "" ;                           // Too long, reset
    }
  }
}


//******************************************************************************************
//                                   M K _ L S A N                                         *
//******************************************************************************************
// Make al list of acceptable networks in .ini file.                                       *
// The result will be stored in anetworks like "|SSID1|SSID2|......|SSIDN|".               *
// The number of acceptable networks will be stored in num_an.                             *
//******************************************************************************************
void mk_lsan()
{
  String      path ;                                   // Full file spec as string
  File        inifile ;                                // File containing URL with mp3
  String      line ;                                   // Input line from .ini file
  String      ssid ;                                   // SSID in line
  int         inx ;                                    // Place of "/"

  num_an = 0 ;                                         // Count acceptable networks
  anetworks = "|" ;                                    // Initial value
  path = String ( INIFILENAME ) ;                      // Form full path
  inifile = SPIFFS.open ( path, "r" ) ;                // Open the file
  if ( inifile )
  {
    while ( inifile.available() )
    {
      line = inifile.readStringUntil ( '\n' ) ;        // Read next line
      ssid = line ;                                    // Copy holds original upper/lower case
      line.toLowerCase() ;                             // Case insensitive
      if ( line.startsWith ( "wifi" ) )                // Line with WiFi spec?
      {
        inx = line.indexOf ( "/" ) ;                   // Find separator between ssid and password
        if ( inx > 0 )                                 // Separator found?
        {
          ssid = ssid.substring ( 5, inx ) ;           // Line holds SSID now
          dbgprint ( "Added SSID %s to acceptable networks",
                     ssid.c_str() ) ;
          anetworks += ssid ;                          // Add to list
          anetworks += "|" ;                           // Separator
          num_an++ ;                                   // Count number oif acceptable networks
        }
      }
    }
    inifile.close() ;                                  // Close the file
  }
  else
  {
    dbgprint ( "File %s not found!", INIFILENAME ) ;   // No .ini file
  }
}


//******************************************************************************************
//                             G E T P R E S E T S                                         *
//******************************************************************************************
// Make a list of all preset stations.                                                     *
// The result will be stored in the String presetlist (global data).                       *
//******************************************************************************************
void getpresets()
{
  String              path ;                             // Full file spec as string
  File                inifile ;                          // File containing URL with mp3
  String              line ;                             // Input line from .ini file
  int                 inx ;                              // Position of search char in line
  int                 i ;                                // Loop control
  char                vnr[3] ;                           // 2 digit presetnumber as string

  presetlist = String ( "" ) ;                           // No result yet
  path = String ( INIFILENAME ) ;                        // Form full path
  inifile = SPIFFS.open ( path, "r" ) ;                  // Open the file
  if ( inifile )
  {
    while ( inifile.available() )
    {
      line = inifile.readStringUntil ( '\n' ) ;          // Read next line
      if ( line.startsWith ( "preset_" ) )               // Found the key?
      {
        i = line.substring(7, 9).toInt() ;               // Get index 00..99
        // Show just comment if available.  Otherwise the preset itself.
        inx = line.indexOf ( "#" ) ;                     // Get position of "#"
        if ( inx > 0 )                                   // Hash sign present?
        {
          line.remove ( 0, inx + 1 ) ;                   // Yes, remove non-comment part
        }
        else
        {
          inx = line.indexOf ( "=" ) ;                   // Get position of "="
          if ( inx > 0 )                                 // Equal sign present?
          {
            line.remove ( 0, inx + 1 ) ;                 // Yes, remove first part of line
          }
        }
        line = chomp ( line ) ;                          // Remove garbage from description
        sprintf ( vnr, "%02d", i ) ;                     // Preset number
        presetlist += ( String ( vnr ) + line +          // 2 digits plus description
                        String ( "|" ) ) ;
      }
    }
    inifile.close() ;                                    // Close the file
  }
}


//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup()
{
  FSInfo      fs_info ;                                // Info about SPIFFS
  Dir         dir ;                                    // Directory struct for SPIFFS
  File        f ;                                      // Filehandle
  String      filename ;                               // Name of file found in SPIFFS

  Serial.begin ( 115200 ) ;                            // For debug
  Serial.println() ;
  system_update_cpu_freq ( 160 ) ;                     // Set to 80/160 MHz
  ringbuf = (uint8_t *) malloc ( RINGBFSIZ ) ;         // Create ring buffer
  xml.init ( xmlbuffer, sizeof(xmlbuffer),             // Initilize XML stream.
             &XML_callback ) ;
  memset ( &ini_block, 0, sizeof(ini_block) ) ;        // Init ini_block
  ini_block.mqttport = 1883 ;                          // Default port for MQTT
  SPIFFS.begin() ;                                     // Enable file system
  // Show some info about the SPIFFS
  SPIFFS.info ( fs_info ) ;
  dbgprint ( "FS Total %d, used %d", fs_info.totalBytes, fs_info.usedBytes ) ;
  if ( fs_info.totalBytes == 0 )
  {
    dbgprint ( "No SPIFFS found!  See documentation." ) ;
  }
  dir = SPIFFS.openDir("/") ;                          // Show files in FS
  while ( dir.next() )                                 // All files
  {
    f = dir.openFile ( "r" ) ;
    filename = dir.fileName() ;
    dbgprint ( "%-32s - %7d",                          // Show name and size
               filename.c_str(), f.size() ) ;
  }
  mk_lsan() ;                                          // Make a list of acceptable networks in ini file.
  listNetworks() ;                                     // Search for WiFi networks
  readinifile() ;                                      // Read .ini file
  getpresets() ;                                       // Get the presets from .ini-file
  WiFi.setPhyMode ( WIFI_PHY_MODE_11N ) ;              // Force 802.11N connection
  WiFi.persistent ( false ) ;                          // Do not save SSID and password
  WiFi.disconnect() ;                                  // The router may keep the old connection
  WiFi.mode ( WIFI_STA ) ;                             // This ESP is a station
  wifi_station_set_hostname ( (char*)NAME ) ;
  SPI.begin() ;                                        // Init SPI bus
  // Print some memory and sketch info
  dbgprint ( "Starting ESP Version %s...  Free memory %d",
             VERSION,
             system_get_free_heap_size() ) ;
  dbgprint ( "Sketch size %d, free size %d",
             ESP.getSketchSize(),
             ESP.getFreeSketchSpace() ) ;
  pinMode ( BUTTON2, INPUT_PULLUP ) ;                  // Input for control button 2
  vs1053player.begin() ;                               // Initialize VS1053 player
#if defined ( USETFT )
  tft.begin() ;                                        // Init TFT interface
  tft.fillRect ( 0, 0, 160, 128, BLACK ) ;             // Clear screen does not work when rotated
  tft.setRotation ( 3 ) ;                              // Use landscape format
  tft.clearScreen() ;                                  // Clear screen
  tft.setTextSize ( 1 ) ;                              // Small character font
  tft.setTextColor ( WHITE ) ;                         // Info in white
  tft.println ( "Starting" ) ;
  tft.println ( "Version:" ) ;
  tft.println ( VERSION ) ;
#else
  pinMode ( BUTTON1, INPUT_PULLUP ) ;                  // Input for control button 1
  pinMode ( BUTTON3, INPUT_PULLUP ) ;                  // Input for control button 3
#endif
  delay(10);
  analogrest = ( analogRead ( A0 ) + asw1 ) / 2  ;     // Assumed inactive analog input
  tckr.attach ( 0.100, timer100 ) ;                    // Every 100 msec
  dbgprint ( "Selected network: %-25s", ini_block.ssid.c_str() ) ;
  NetworkFound = connectwifi() ;                       // Connect to WiFi network
  //NetworkFound = false ;                             // TEST, uncomment for no network test
  dbgprint ( "Start server for commands" ) ;
  cmdserver.on ( "/", handleCmd ) ;                    // Handle startpage
  cmdserver.onNotFound ( handleFS ) ;                  // Handle file from FS
  cmdserver.onFileUpload ( handleFileUpload ) ;        // Handle file uploads
  cmdserver.begin() ;
  if ( NetworkFound )                                  // OTA and MQTT only if Wifi network found
  {
    ArduinoOTA.setHostname ( NAME ) ;                  // Set the hostname
    ArduinoOTA.onStart ( otastart ) ;
    ArduinoOTA.begin() ;                               // Allow update over the air
    if ( ini_block.mqttbroker.length() )               // Broker specified?
    {
      // Initialize the MQTT client
      WiFi.hostByName ( ini_block.mqttbroker.c_str(),
                        mqtt_server_IP ) ;             // Lookup IP of MQTT server
      mqttclient.onConnect ( onMqttConnect ) ;
      mqttclient.onDisconnect ( onMqttDisconnect ) ;
      mqttclient.onSubscribe ( onMqttSubscribe ) ;
      mqttclient.onUnsubscribe ( onMqttUnsubscribe ) ;
      mqttclient.onMessage ( onMqttMessage ) ;
      mqttclient.onPublish ( onMqttPublish ) ;
      mqttclient.setServer ( mqtt_server_IP,           // Specify the broker
                             ini_block.mqttport ) ;    // And the port
      mqttclient.setCredentials ( ini_block.mqttuser.c_str(),
                                  ini_block.mqttpasswd.c_str() ) ;
      mqttclient.setClientId ( NAME ) ;
      dbgprint ( "Connecting to MQTT %s, port %d, user %s, password %s...",
                 ini_block.mqttbroker.c_str(),
                 ini_block.mqttport,
                 ini_block.mqttuser.c_str(),
                 ini_block.mqttpasswd.c_str() ) ;
      mqttclient.connect();
    }
  }
  else
  {
    currentpreset = ini_block.newpreset ;              // No network: do not start radio
  }
  delay ( 1000 ) ;                                     // Show IP for a wile
  analogrest = ( analogRead ( A0 ) + asw1 ) / 2  ;     // Assumed inactive analog input
}


//******************************************************************************************
//                                  X M L  C A L L B A C K                                 *
//******************************************************************************************
// Process XML tags into variables.                                                        *
//******************************************************************************************
void XML_callback ( uint8_t statusflags, char* tagName, uint16_t tagNameLen,
                    char* data,  uint16_t dataLen )
{
  if ( statusflags & STATUS_START_TAG )
  {
    if ( tagNameLen )
    {
      xmlOpen = String ( tagName ) ;
      //dbgprint ( "Start tag %s",tagName ) ;
    }
  }
  else if ( statusflags & STATUS_END_TAG )
  {
    //dbgprint ( "End tag %s", tagName ) ;
  }
  else if ( statusflags & STATUS_TAG_TEXT )
  {
    xmlTag = String( tagName ) ;
    xmlData = String( data ) ;
    //dbgprint ( Serial.print( "Tag: %s, text: %s", tagName, data ) ;
  }
  else if ( statusflags & STATUS_ATTR_TEXT )
  {
    //dbgprint ( "Attribute: %s, text: %s", tagName, data ) ;
  }
  else if  ( statusflags & STATUS_ERROR )
  {
    //dbgprint ( "XML Parsing error  Tag: %s, text: %s", tagName, data ) ;
  }
}


//******************************************************************************************
//                                  X M L  P A R S E                                       *
//******************************************************************************************
// Parses streams from XML data.                                                           *
//******************************************************************************************
String xmlparse ( String mount )
{
  // Example URL for XML Data Stream:
  // http://playerservices.streamtheworld.com/api/livestream?version=1.5&mount=IHR_TRANAAC&lang=en
  // Clear all variables for use.
  char   tmpstr[200] ;                              // Full GET command, later stream URL
  char   c ;                                        // Next input character from reply
  String urlout ;                                   // Result URL
  bool   urlfound = false ;                         // Result found

  stationServer = "" ;
  stationPort = "" ;
  stationMount = "" ;
  xmlTag = "" ;
  xmlData = "" ;
  stop_mp3client() ; // Stop any current wificlient connections.
  dbgprint ( "Connect to new iHeartRadio host: %s", mount.c_str() ) ;
  datamode = INIT ;                                 // Start default in metamode
  chunked = false ;                                 // Assume not chunked
  // Create a GET commmand for the request.
  sprintf ( tmpstr, xmlget, mount.c_str() ) ;
  dbgprint ( "%s", tmpstr ) ;
  // Connect to XML stream.
  mp3client = new WiFiClient() ;
  if ( mp3client->connect ( xmlhost, xmlport ) ) {
    dbgprint ( "Connected!" ) ;
    mp3client->print ( String ( tmpstr ) + " HTTP/1.1\r\n"
                       "Host: " + xmlhost + "\r\n"
                       "User-Agent: Mozilla/5.0\r\n"
                       "Connection: close\r\n\r\n" ) ;
    // Check for XML Data.
    while ( true )
    {
      if ( mp3client->available() )
      {
        char c = mp3client->read() ;
        if ( c == '<' )
        {
          c = mp3client->read() ;
          if ( c == '?' )
          {
            xml.processChar ( '<' ) ;
            xml.processChar ( '?' ) ;
            break ;
          }
        }
      }
      yield() ;
    }
    dbgprint ( "XML parser processing..." ) ;
    // Process XML Data.
    while (true) 
    {
      if ( mp3client->available() )
      {
        c = mp3client->read() ;
        xml.processChar ( c ) ;
        if ( xmlTag != "" )
        {
          if ( xmlTag.endsWith ( "/status-code" ) )   // Status code seen?
          {
            if ( xmlData != "200" )                   // Good result?
            {
              dbgprint ( "Bad xml status-code %s",    // No, show and stop interpreting
                          xmlData.c_str() ) ;
              break ;
            }
          }
          if ( xmlTag.endsWith ( "/ip" ) )
          {
            stationServer = xmlData ;
          }
          else if ( xmlTag.endsWith ( "/port" ) )
          {
            stationPort = xmlData ;
          }
          else if ( xmlTag.endsWith ( "/mount"  ) )
          {
            stationMount = xmlData ;
          }
        }
      }
      // Check if all the station values are stored.
      urlfound = ( stationServer != "" && stationPort != "" && stationMount != "" ) ;
      if ( urlfound )
      {
        xml.reset() ;
        break ;
      }
      yield() ;
    }
    tmpstr[0] = '\0' ;                            
    if ( urlfound )
    {
      sprintf ( tmpstr, "%s:%s/%s_SC",                   // Build URL for ESP-Radio to stream.
                        stationServer.c_str(),
                        stationPort.c_str(),
                        stationMount.c_str() ) ;
      dbgprint ( "Found: %s", tmpstr ) ;
    }
    dbgprint ( "Closing XML connection." ) ;
    stop_mp3client () ;
  }
  else
  {
    dbgprint ( "Can't connect to XML host!" ) ;
    tmpstr[0] = '\0' ;                            
  }
  return String ( tmpstr ) ;                           // Return final streaming URL.
}


//******************************************************************************************
//                                   L O O P                                               *
//******************************************************************************************
// Main loop of the program.  Minimal time is 20 usec.  Will take about 4 msec if VS1053   *
// needs data.                                                                             *
// Sometimes the loop is called after an interval of more than 100 msec.                   *
// In that case we will not be able to fill the internal VS1053-fifo in time (especially   *
// at high bitrate).                                                                       *
// A connection to an MP3 server is active and we are ready to receive data.               *
// Normally there is about 2 to 4 kB available in the data stream.  This depends on the    *
// sender.                                                                                 *
//******************************************************************************************
void loop()
{
  uint32_t    maxfilechunk  ;                           // Max number of bytes to read from
                                                        // stream or file
  // Try to keep the ringbuffer filled up by adding as much bytes as possible
  if ( datamode & ( INIT | HEADER | DATA |              // Test op playing
                    METADATA | PLAYLISTINIT |
                    PLAYLISTHEADER |
                    PLAYLISTDATA ) )
  {
    if ( localfile )
    {
      maxfilechunk = mp3file.available() ;              // Bytes left in file
      if ( maxfilechunk > 1024 )                        // Reduce byte count for this loop()
      {
        maxfilechunk = 1024 ;
      }
      while ( ringspace() && maxfilechunk-- )
      {
        putring ( mp3file.read() ) ;                    // Yes, store one byte in ringbuffer
        yield() ;
      }
    }
    else
    {
      maxfilechunk = mp3client->available() ;          // Bytes available from mp3 server
      if ( maxfilechunk > 1024 )                       // Reduce byte count for this loop()
      {
        maxfilechunk = 1024 ;
      }
      while ( ringspace() && maxfilechunk-- )
      {
        putring ( mp3client->read() ) ;                // Yes, store one byte in ringbuffer
        yield() ;
      }
    }
    yield() ;
  }
  while ( vs1053player.data_request() && ringavail() ) // Try to keep VS1053 filled
  {
    handlebyte_ch ( getring() ) ;                      // Yes, handle it
  }
  yield() ;
  if ( datamode == STOPREQD )                          // STOP requested?
  {
    dbgprint ( "STOP requested" ) ;
    if ( localfile )
    {
      mp3file.close() ;
    }
    else
    {
      stop_mp3client() ;                               // Disconnect if still connected
    }
    handlebyte_ch ( 0, true ) ;                        // Force flush of buffer
    vs1053player.setVolume ( 0 ) ;                     // Mute
    vs1053player.stopSong() ;                          // Stop playing
    emptyring() ;                                      // Empty the ringbuffer
    datamode = STOPPED ;                               // Yes, state becomes STOPPED
#if defined ( USETFT )
    tft.fillRect ( 0, 0, 160, 128, BLACK ) ;           // Clear screen does not work when rotated
#endif
    delay ( 500 ) ;
  }
  if ( localfile )
  {
    if ( datamode & ( INIT | HEADER | DATA |           // Test op playing
                      METADATA | PLAYLISTINIT |
                      PLAYLISTHEADER |
                      PLAYLISTDATA ) )
    {
      if ( ( mp3file.available() == 0 ) && ( ringavail() == 0 ) )
      {
        datamode = STOPREQD ;                          // End of local mp3-file detected
      }
    }
  }
  if ( ini_block.newpreset != currentpreset )          // New station or next from playlist requested?
  {
    if ( datamode != STOPPED )                         // Yes, still busy?
    {
      datamode = STOPREQD ;                            // Yes, request STOP
    }
    else
    {
      if ( playlist_num )                               // Playing from playlist?
      { // Yes, retrieve URL of playlist
        playlist_num += ini_block.newpreset -
                        currentpreset ;                 // Next entry in playlist
        ini_block.newpreset = currentpreset ;           // Stay at current preset
      }
      else
      {
        host = readhostfrominifile(ini_block.newpreset) ; // Lookup preset in ini-file
      }
      dbgprint ( "New preset/file requested (%d/%d) from %s",
                 currentpreset, playlist_num, host.c_str() ) ;
      if ( host != ""  )                                // Preset in ini-file?
      {
        hostreq = true ;                                // Force this station as new preset
      }
      else
      {
        // This preset is not available, return to preset 0, will be handled in next loop()
        ini_block.newpreset = 0 ;                       // Wrap to first station
      }
    }
  }
  if ( hostreq )                                        // New preset or station?
  {
    hostreq = false ;
    currentpreset = ini_block.newpreset ;               // Remember current preset
    
    localfile = host.startsWith ( "localhost/" ) ;      // Find out if this URL is on localhost
    if ( localfile )                                    // Play file from localhost?
    {
      if ( connecttofile() )                            // Yes, open mp3-file
      {
        datamode = DATA ;                               // Start in DATA mode
      }
    }
    else
    {
      if ( host.startsWith ( "ihr/" ) )                 // iHeartRadio station requested?
      {
        host = host.substring ( 4 ) ;                   // Yes, remove "ihr/"
        host = xmlparse ( host ) ;                      // Parse the xml to get the host
      }
      connecttohost() ;                                 // Switch to new host
    }
  }
  if ( xmlreq )                                         // Directly xml requested?
  {
    xmlreq = false ;                                    // Yes, clear request flag
    host = xmlparse ( host ) ;                          // Parse the xml to get the host
    connecttohost() ;                                   // and connect to this host
  }
  if ( reqtone )                                        // Request to change tone?
  {
    reqtone = false ;
    vs1053player.setTone ( ini_block.rtone ) ;          // Set SCI_BASS to requested value
  }
  if ( resetreq )                                       // Reset requested?
  {
    delay ( 1000 ) ;                                    // Yes, wait some time
    ESP.restart() ;                                     // Reboot
  }
  if ( muteflag )
  {
    vs1053player.setVolume ( 0 ) ;                      // Mute
  }
  else
  {
    vs1053player.setVolume ( ini_block.reqvol ) ;       // Unmute
  }
  displayvolume() ;                                     // Show volume on display
  if ( testfilename.length() )                          // File to test?
  {
    testfile ( testfilename ) ;                         // Yes, do the test
    testfilename = "" ;                                 // Clear test request
  }
  scanserial() ;                                        // Handle serial input
  ArduinoOTA.handle() ;                                 // Check for OTA
}


//******************************************************************************************
//                            C H K H D R L I N E                                          *
//******************************************************************************************
// Check if a line in the header is a reasonable headerline.                               *
// Normally it should contain something like "icy-xxxx:abcdef".                            *
//******************************************************************************************
bool chkhdrline ( const char* str )
{
  char    b ;                                         // Byte examined
  int     len = 0 ;                                   // Lengte van de string

  while ( ( b = *str++ ) )                            // Search to end of string
  {
    len++ ;                                           // Update string length
    if ( ! isalpha ( b ) )                            // Alpha (a-z, A-Z)
    {
      if ( b != '-' )                                 // Minus sign is allowed
      {
        if ( b == ':' )                               // Found a colon?
        {
          return ( ( len > 5 ) && ( len < 50 ) ) ;    // Yes, okay if length is okay
        }
        else
        {
          return false ;                              // Not a legal character
        }
      }
    }
  }
  return false ;                                      // End of string without colon
}


//******************************************************************************************
//                           H A N D L E B Y T E _ C H                                     *
//******************************************************************************************
// Handle the next byte of data from server.                                               *
// Chunked transfer encoding aware. Chunk extensions are not supported.                    *
//******************************************************************************************
void handlebyte_ch ( uint8_t b, bool force )
{
  static int  chunksize = 0 ;                         // Chunkcount read from stream

  if ( chunked && !force && 
       ( datamode & ( DATA |                          // Test op DATA handling
                      METADATA |
                      PLAYLISTDATA ) ) )
  {
    if ( chunkcount == 0 )                            // Expecting a new chunkcount?
    {
       if ( b == '\r' )                               // Skip CR
       {
         return ;      
       }
       else if ( b == '\n' )                          // LF ?
       {
         chunkcount = chunksize ;                     // Yes, set new count
         chunksize = 0 ;                              // For next decode
         return ;
       }
       // We have received a hexadecimal character.  Decode it and add to the result.
       b = toupper ( b ) - '0' ;                      // Be sure we have uppercase
       if ( b > 9 )
       {
         b = b - 7 ;                                  // Translate A..F to 10..15
       }
       chunksize = ( chunksize << 4 ) + b ;
    }
    else
    {
      handlebyte ( b, force ) ;                       // Normal data byte
      chunkcount-- ;                                  // Update count to next chunksize block
    }
  }
  else
  {
    handlebyte ( b, force ) ;                         // Normal handling of this byte
  }
}


//******************************************************************************************
//                           H A N D L E B Y T E                                           *
//******************************************************************************************
// Handle the next byte of data from server.                                               *
// This byte will be send to the VS1053 most of the time.                                  *
// Note that the buffer the data chunk must start at an address that is a muttiple of 4.   *
// Set force to true if chunkbuffer must be flushed.                                       *
//******************************************************************************************
void handlebyte ( uint8_t b, bool force )
{
  static uint16_t  playlistcnt ;                       // Counter to find right entry in playlist
  static bool      firstmetabyte ;                     // True if first metabyte (counter)
  static int       LFcount ;                           // Detection of end of header
  static __attribute__((aligned(4))) uint8_t buf[32] ; // Buffer for chunk
  static int       bufcnt = 0 ;                        // Data in chunk
  static bool      firstchunk = true ;                 // First chunk as input
  String           lcml ;                              // Lower case metaline
  String           ct ;                                // Contents type
  static bool      ctseen = false ;                    // First line of header seen or not
  int              inx ;                               // Pointer in metaline
  int              i ;                                 // Loop control

  if ( datamode == INIT )                              // Initialize for header receive
  {
    ctseen = false ;                                   // Contents type not seen yet
    metaint = 0 ;                                      // No metaint found
    LFcount = 0 ;                                      // For detection end of header
    bitrate = 0 ;                                      // Bitrate still unknown
    dbgprint ( "Switch to HEADER" ) ;
    datamode = HEADER ;                                // Handle header
    totalcount = 0 ;                                   // Reset totalcount
    metaline = "" ;                                    // No metadata yet
    firstchunk = true ;                                // First chunk expected
  }
  if ( datamode == DATA )                              // Handle next byte of MP3/Ogg data
  {
    buf[bufcnt++] = b ;                                // Save byte in chunkbuffer
    if ( bufcnt == sizeof(buf) || force )              // Buffer full?
    {
      if ( firstchunk )
      {
        firstchunk = false ;
        dbgprint ( "First chunk:" ) ;                  // Header for printout of first chunk
        for ( i = 0 ; i < 32 ; i += 8 )                // Print 4 lines
        {
          dbgprint ( "%02X %02X %02X %02X %02X %02X %02X %02X",
                     buf[i],   buf[i + 1], buf[i + 2], buf[i + 3],
                     buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7] ) ;
        }
      }
      vs1053player.playChunk ( buf, bufcnt ) ;         // Yes, send to player
      bufcnt = 0 ;                                     // Reset count
    }
    totalcount++ ;                                     // Count number of bytes, ignore overflow
    if ( metaint != 0 )                                // No METADATA on Ogg streams or mp3 files
    {
      if ( --datacount == 0 )                          // End of datablock?
      {
        if ( bufcnt )                                  // Yes, still data in buffer?
        {
          vs1053player.playChunk ( buf, bufcnt ) ;     // Yes, send to player
          bufcnt = 0 ;                                 // Reset count
        }
        datamode = METADATA ;
        firstmetabyte = true ;                         // Expecting first metabyte (counter)
      }
    }
    return ;
  }
  if ( datamode == HEADER )                            // Handle next byte of MP3 header
  {
    if ( ( b > 0x7F ) ||                               // Ignore unprintable characters
         ( b == '\r' ) ||                              // Ignore CR
         ( b == '\0' ) )                               // Ignore NULL
    {
      // Yes, ignore
    }
    else if ( b == '\n' )                              // Linefeed ?
    {
      LFcount++ ;                                      // Count linefeeds
      if ( chkhdrline ( metaline.c_str() ) )           // Reasonable input?
      {
        lcml = metaline ;                              // Use lower case for compare
        lcml.toLowerCase() ;
        dbgprint ( metaline.c_str() ) ;                // Yes, Show it
        if ( lcml.indexOf ( "content-type" ) >= 0 )    // Line with "Content-Type: xxxx/yyy"
        {
          ctseen = true ;                              // Yes, remember seeing this
          ct = metaline.substring ( 14 ) ;             // Set contentstype. Not used yet
          dbgprint ( "%s seen.", ct.c_str() ) ;
        }
        if ( lcml.startsWith ( "icy-br:" ) )
        {
          bitrate = metaline.substring(7).toInt() ;    // Found bitrate tag, read the bitrate
          if ( bitrate == 0 )                          // For Ogg br is like "Quality 2"
          {
            bitrate = 87 ;                             // Dummy bitrate
          }
        }
        else if ( lcml.startsWith ( "icy-metaint:" ) )
        {
          metaint = metaline.substring(12).toInt() ;   // Found metaint tag, read the value
        }
        else if ( lcml.startsWith ( "icy-name:" ) )
        {
          icyname = metaline.substring(9) ;            // Get station name
          icyname.trim() ;                             // Remove leading and trailing spaces
          displayinfo ( icyname.c_str(), 60, 68,
                        YELLOW ) ;                     // Show station name at position 60
        }
        else if ( lcml.startsWith ( "transfer-encoding:" ) )
        {
          // Station provides chunked transfer
          if ( lcml.endsWith ( "chunked" ) )
          {
            chunked = true ;                           // Remember chunked transfer mode
            chunkcount = 0 ;                           // Expect chunkcount in DATA
          }
        }
      }
      metaline = "" ;                                  // Reset this line
      if ( ( LFcount == 2 ) && ctseen )                // Some data seen and a double LF?
      {
        dbgprint ( "Switch to DATA, bitrate is %d"     // Show bitrate
                   ", metaint is %d",                  // and metaint
                   bitrate, metaint ) ;
        datamode = DATA ;                              // Expecting data now
        datacount = metaint ;                          // Number of bytes before first metadata
        bufcnt = 0 ;                                   // Reset buffer count
        vs1053player.startSong() ;                     // Start a new song
      }
    }
    else
    {
      metaline += (char)b ;                            // Normal character, put new char in metaline
      LFcount = 0 ;                                    // Reset double CRLF detection
    }
    return ;
  }
  if ( datamode == METADATA )                          // Handle next byte of metadata
  {
    if ( firstmetabyte )                               // First byte of metadata?
    {
      firstmetabyte = false ;                          // Not the first anymore
      metacount = b * 16 + 1 ;                         // New count for metadata including length byte
      if ( metacount > 1 )
      {
        dbgprint ( "Metadata block %d bytes",
                   metacount - 1 ) ;                   // Most of the time there are zero bytes of metadata
      }
      metaline = "" ;                                  // Set to empty
    }
    else
    {
      metaline += (char)b ;                            // Normal character, put new char in metaline
    }
    if ( --metacount == 0 )
    {
      if ( metaline.length() )                         // Any info present?
      {
        // metaline contains artist and song name.  For example:
        // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
        // Sometimes it is just other info like:
        // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
        // Isolate the StreamTitle, remove leading and trailing quotes if present.
        showstreamtitle ( metaline.c_str() ) ;         // Show artist and title if present in metadata
      }
      if ( metaline.length() > 1500 )                  // Unlikely metaline length?
      {
        dbgprint ( "Metadata block too long! Skipping all Metadata from now on." ) ;
        metaint = 0 ;                                  // Probably no metadata
        metaline = "" ;                                // Do not waste memory on this
      }
      datacount = metaint ;                            // Reset data count
      bufcnt = 0 ;                                     // Reset buffer count
      datamode = DATA ;                                // Expecting data
    }
  }
  if ( datamode == PLAYLISTINIT )                      // Initialize for receive .m3u file
  {
    // We are going to use metadata to read the lines from the .m3u file
    metaline = "" ;                                    // Prepare for new line
    LFcount = 0 ;                                      // For detection end of header
    datamode = PLAYLISTHEADER ;                        // Handle playlist data
    playlistcnt = 1 ;                                  // Reset for compare
    totalcount = 0 ;                                   // Reset totalcount
    dbgprint ( "Read from playlist" ) ;
  }
  if ( datamode == PLAYLISTHEADER )                    // Read header
  {
    if ( ( b > 0x7F ) ||                               // Ignore unprintable characters
         ( b == '\r' ) ||                              // Ignore CR
         ( b == '\0' ) )                               // Ignore NULL
    {
      // Yes, ignore
    }
    else if ( b == '\n' )                              // Linefeed ?
    {
      LFcount++ ;                                      // Count linefeeds
      dbgprint ( "Playlistheader: %s",                 // Show playlistheader
                 metaline.c_str() ) ;
      metaline = "" ;                                  // Ready for next line
      if ( LFcount == 2 )
      {
        dbgprint ( "Switch to PLAYLISTDATA" ) ;
        datamode = PLAYLISTDATA ;                      // Expecting data now
        return ;
      }
    }
    else
    {
      metaline += (char)b ;                            // Normal character, put new char in metaline
      LFcount = 0 ;                                    // Reset double CRLF detection
    }
  }
  if ( datamode == PLAYLISTDATA )                      // Read next byte of .m3u file data
  {
    if ( ( b > 0x7F ) ||                               // Ignore unprintable characters
         ( b == '\r' ) ||                              // Ignore CR
         ( b == '\0' ) )                               // Ignore NULL
    {
      // Yes, ignore
    }
    else if ( b == '\n' )                              // Linefeed ?
    {
      dbgprint ( "Playlistdata: %s",                   // Show playlistheader
                 metaline.c_str() ) ;
      if ( metaline.length() < 5 )                     // Skip short lines
      {
        return ;
      }
      if ( metaline.indexOf ( "#EXTINF:" ) >= 0 )      // Info?
      {
        if ( playlist_num == playlistcnt )             // Info for this entry?
        {
          inx = metaline.indexOf ( "," ) ;             // Comma in this line?
          if ( inx > 0 )
          {
            // Show artist and title if present in metadata
            showstreamtitle ( metaline.substring ( inx + 1 ).c_str(), true ) ;
          }
        }
      }
      if ( metaline.startsWith ( "#" ) )               // Commentline?
      {
        metaline = "" ;
        return ;                                       // Ignore commentlines
      }
      // Now we have an URL for a .mp3 file or stream.  Is it the rigth one?
      dbgprint ( "Entry %d in playlist found: %s", playlistcnt, metaline.c_str() ) ;
      if ( playlist_num == playlistcnt  )
      {
        inx = metaline.indexOf ( "http://" ) ;         // Search for "http://"
        if ( inx >= 0 )                                // Does URL contain "http://"?
        {
          host = metaline.substring ( inx + 7 ) ;      // Yes, remove it and set host
        }
        else
        {
          host = metaline ;                            // Yes, set new host
        }
        connecttohost() ;                              // Connect to it
      }
      metaline = "" ;
      host = playlist ;                                // Back to the .m3u host
      playlistcnt++ ;                                  // Next entry in playlist
    }
    else
    {
      metaline += (char)b ;                            // Normal character, add it to metaline
    }
    return ;
  }
}


//******************************************************************************************
//                             G E T C O N T E N T T Y P E                                 *
//******************************************************************************************
// Returns the contenttype of a file to send.                                              *
//******************************************************************************************
String getContentType ( String filename )
{
  if      ( filename.endsWith ( ".html" ) ) return "text/html" ;
  else if ( filename.endsWith ( ".png"  ) ) return "image/png" ;
  else if ( filename.endsWith ( ".gif"  ) ) return "image/gif" ;
  else if ( filename.endsWith ( ".jpg"  ) ) return "image/jpeg" ;
  else if ( filename.endsWith ( ".ico"  ) ) return "image/x-icon" ;
  else if ( filename.endsWith ( ".css"  ) ) return "text/css" ;
  else if ( filename.endsWith ( ".zip"  ) ) return "application/x-zip" ;
  else if ( filename.endsWith ( ".gz"   ) ) return "application/x-gzip" ;
  else if ( filename.endsWith ( ".mp3"  ) ) return "audio/mpeg" ;
  else if ( filename.endsWith ( ".pw"   ) ) return "" ;              // Passwords are secret
  return "text/plain" ;
}


//******************************************************************************************
//                         H A N D L E F I L E U P L O A D                                 *
//******************************************************************************************
// Handling of upload request.  Write file to SPIFFS.                                      *
//******************************************************************************************
void handleFileUpload ( AsyncWebServerRequest *request, String filename,
                        size_t index, uint8_t *data, size_t len, bool final )
{
  String          path ;                              // Filename including "/"
  static File     f ;                                 // File handle output file
  char*           reply ;                             // Reply for webserver
  static uint32_t t ;                                 // Timer for progress messages
  uint32_t        t1 ;                                // For compare
  static uint32_t totallength ;                       // Total file length
  static size_t   lastindex ;                         // To test same index

  if ( index == 0 )
  {
    path = String ( "/" ) + filename ;                // Form SPIFFS filename
    SPIFFS.remove ( path ) ;                          // Remove old file
    f = SPIFFS.open ( path, "w" ) ;                   // Create new file
    t = millis() ;                                    // Start time
    totallength = 0 ;                                 // Total file lengt still zero
    lastindex = 0 ;                                   // Prepare test
  }
  t1 = millis() ;                                     // Current timestamp
  // Yes, print progress
  dbgprint ( "File upload %s, t = %d msec, len %d, index %d",
               filename.c_str(), t1 - t, len, index ) ;
  if ( len )                                          // Something to write?
  {
    if ( ( index != lastindex ) || ( index == 0 ) )   // New chunk?
    {
      f.write ( data, len ) ;                         // Yes, transfer to SPIFFS
      totallength += len ;                            // Update stored length
      lastindex = index ;                             // Remenber this part
    }
  }
  if ( final )                                        // Was this last chunk?
  {
    f.close() ;                                       // Yes, clode the file
    reply = dbgprint ( "File upload %s, %d bytes finished",
                       filename.c_str(), totallength ) ;
    request->send ( 200, "", reply ) ;
  }
}


//******************************************************************************************
//                                H A N D L E F S F                                        *
//******************************************************************************************
// Handling of requesting files from the SPIFFS/PROGMEM. Example: /favicon.ico             *
//******************************************************************************************
void handleFSf ( AsyncWebServerRequest* request, const String& filename )
{
  static String          ct ;                           // Content type
  AsyncWebServerResponse *response ;                    // For extra headers

  dbgprint ( "FileRequest received %s", filename.c_str() ) ;
  ct = getContentType ( filename ) ;                    // Get content type
  if ( ( ct == "" ) || ( filename == "" ) )             // Empty is illegal
  {
    request->send ( 404, "text/plain", "File not found" ) ;
  }
  else
  {
    if ( filename.indexOf ( "index.html" ) >= 0 )       // Index page is in PROGMEM
    {
      response = request->beginResponse_P ( 200, ct, index_html ) ;
    }
    else if ( filename.indexOf ( "radio.css" ) >= 0 )   // CSS file is in PROGMEM
    {
      response = request->beginResponse_P ( 200, ct, radio_css ) ;
    }
    else if ( filename.indexOf ( "config.html" ) >= 0 ) // Config page is in PROGMEM
    {
      response = request->beginResponse_P ( 200, ct, config_html ) ;
    }
    else if ( filename.indexOf ( "about.html" ) >= 0 )  // About page is in PROGMEM
    {
      response = request->beginResponse_P ( 200, ct, about_html ) ;
    }
    else if ( filename.indexOf ( "favicon.ico" ) >= 0 ) // Favicon icon is in PROGMEM
    {
      response = request->beginResponse_P ( 200, ct, favicon_ico, sizeof ( favicon_ico ) ) ;
    }
    else
    {
      response = request->beginResponse ( SPIFFS, filename, ct ) ;
    }
    // Add extra headers
    response->addHeader ( "Server", NAME ) ;
    response->addHeader ( "Cache-Control", "max-age=3600" ) ;
    response->addHeader ( "Last-Modified", VERSION ) ;
    request->send ( response ) ;
  }
  dbgprint ( "Response sent" ) ;
}


//******************************************************************************************
//                                H A N D L E F S                                          *
//******************************************************************************************
// Handling of requesting files from the SPIFFS. Example: /favicon.ico                     *
//******************************************************************************************
void handleFS ( AsyncWebServerRequest* request )
{
  handleFSf ( request, request->url() ) ;               // Rest of handling
}


//******************************************************************************************
//                             A N A L Y Z E C M D                                         *
//******************************************************************************************
// Handling of the various commands from remote webclient, Serial or MQTT.                 *
// Version for handling string with: <parameter>=<value>                                   *
//******************************************************************************************
char* analyzeCmd ( const char* str )
{
  char*  value ;                                 // Points to value after equalsign in command

  value = strstr ( str, "=" ) ;                  // See if command contains a "="
  if ( value )
  {
    *value = '\0' ;                              // Separate command from value
    value++ ;                                    // Points to value after "="
  }
  else
  {
    value = (char*) "0" ;                        // No value, assume zero
  }
  return  analyzeCmd ( str, value ) ;            // Analyze command and handle it
}


//******************************************************************************************
//                                 C H O M P                                               *
//******************************************************************************************
// Do some filtering on de inputstring:                                                    *
//  - String comment part (starting with "#").                                             *
//  - Strip trailing CR.                                                                   *
//  - Strip leading spaces.                                                                *
//  - Strip trailing spaces.                                                               *
//******************************************************************************************
String chomp ( String str )
{
  int   inx ;                                         // Index in de input string

  if ( ( inx = str.indexOf ( "#" ) ) >= 0 )           // Comment line or partial comment?
  {
    str.remove ( inx ) ;                              // Yes, remove
  }
  str.trim() ;                                        // Remove spaces and CR
  return str ;                                        // Return the result
}


//******************************************************************************************
//                             A N A L Y Z E C M D                                         *
//******************************************************************************************
// Handling of the various commands from remote webclient, serial or MQTT.                 *
// par holds the parametername and val holds the value.                                    *
// "wifi_00" and "preset_00" may appear more than once, like wifi_01, wifi_02, etc.        *
// Examples with available parameters:                                                     *
//   preset     = 12                        // Select start preset to connect to           *
//   preset_00  = <mp3 stream>              // Specify station for a preset 00-99 *)       *
//   volume     = 95                        // Percentage between 0 and 100                *
//   upvolume   = 2                         // Add percentage to current volume            *
//   downvolume = 2                         // Subtract percentage from current volume     *
//   toneha     = <0..15>                   // Setting treble gain                         *
//   tonehf     = <0..15>                   // Setting treble frequency                    *
//   tonela     = <0..15>                   // Setting bass gain                           *
//   tonelf     = <0..15>                   // Setting treble frequency                    *
//   station    = <mp3 stream>              // Select new station (will not be saved)      *
//   station    = <URL>.mp3                 // Play standalone .mp3 file (not saved)       *
//   station    = <URL>.m3u                 // Select playlist (will not be saved)         *
//   stop                                   // Stop playing                                *
//   resume                                 // Resume playing                              *
//   mute                                   // Mute the music                              *
//   unmute                                 // Unmute the music                            *
//   wifi_00    = mySSID/mypassword         // Set WiFi SSID and password *)               *
//   mqttbroker = mybroker.com              // Set MQTT broker to use *)                   *
//   mqttport   = 1883                      // Set MQTT port to use, default 1883 *)       *
//   mqttuser   = myuser                    // Set MQTT user for authentication *)         *
//   mqttpasswd = mypassword                // Set MQTT password for authentication *)     *
//   mqtttopic  = mytopic                   // Set MQTT topic to subscribe to *)           *
//   mqttpubtopic = mypubtopic              // Set MQTT topic to publish to *)             *
//   status                                 // Show current URL to play                    *
//   testfile   = <file on SPIFFS>          // Test SPIFFS reads for debugging purpose     *
//   test                                   // For test purposes                           *
//   debug      = 0 or 1                    // Switch debugging on or off                  *
//   reset                                  // Restart the ESP8266                         *
//   analog                                 // Show current analog input                   *
// Commands marked with "*)" are sensible in ini-file only                                 *
// Note that it is adviced to avoid expressions as the argument for the abs function.      *
//******************************************************************************************
char* analyzeCmd ( const char* par, const char* val )
{
  String             argument ;                       // Argument as string
  String             value ;                          // Value of an argument as a string
  int                ivalue ;                         // Value of argument as an integer
  static char        reply[250] ;                     // Reply to client, will be returned
  uint8_t            oldvol ;                         // Current volume
  bool               relative ;                       // Relative argument (+ or -)
  int                inx ;                            // Index in string

  strcpy ( reply, "Command accepted" ) ;              // Default reply
  argument = chomp ( par ) ;                          // Get the argument
  if ( argument.length() == 0 )                       // Lege commandline (comment)?
  {
    return reply ;                                    // Ignore
  }
  argument.toLowerCase() ;                            // Force to lower case
  value = chomp ( val ) ;                             // Get the specified value
  ivalue = value.toInt() ;                            // Also as an integer
  ivalue = abs ( ivalue ) ;                           // Make it absolute
  relative = argument.indexOf ( "up" ) == 0 ;         // + relative setting?
  if ( argument.indexOf ( "down" ) == 0 )             // - relative setting?
  {
    relative = true ;                                 // It's relative
    ivalue = - ivalue ;                               // But with negative value
  }
  if ( value.startsWith ( "http://" ) )               // Does (possible) URL contain "http://"?
  {
    value.remove ( 0, 7 ) ;                           // Yes, remove it
  }
  if ( value.length() )
  {
    dbgprint ( "Command: %s with parameter %s",
               argument.c_str(), value.c_str() ) ;
  }
  else
  {
    dbgprint ( "Command: %s (without parameter)",
               argument.c_str() ) ;
  }
  if ( argument.indexOf ( "volume" ) >= 0 )           // Volume setting?
  {
    // Volume may be of the form "upvolume", "downvolume" or "volume" for relative or absolute setting
    oldvol = vs1053player.getVolume() ;               // Get current volume
    if ( relative )                                   // + relative setting?
    {
      ini_block.reqvol = oldvol + ivalue ;            // Up by 0.5 or more dB
    }
    else
    {
      ini_block.reqvol = ivalue ;                     // Absolue setting
    }
    if ( ini_block.reqvol > 100 )
    {
      ini_block.reqvol = 100 ;                        // Limit to normal values
    }
    sprintf ( reply, "Volume is now %d",              // Reply new volume
              ini_block.reqvol ) ;
  }
  else if ( argument == "mute" )                      // Mute request
  {
    muteflag = true ;                                 // Request volume to zero
  }
  else if ( argument == "unmute" )                    // Unmute request?
  {
    muteflag = false ;                                // Request normal volume
  }
  else if ( argument.indexOf ( "preset" ) >= 0 )      // Preset station?
  {
    if ( !argument.startsWith ( "preset_" ) )         // But not a station URL
    {
      if ( relative )                                 // Relative argument?
      {
        ini_block.newpreset += ivalue ;               // Yes, adjust currentpreset
      }
      else
      {
        ini_block.newpreset = ivalue ;                // Otherwise set preset station
      }
      sprintf ( reply, "Preset is now %d",            // Reply new preset
                ini_block.newpreset ) ;
      playlist_num = 0 ;
    }
  }
  else if ( argument == "stop" )                      // Stop requested?
  {
    if ( datamode & ( HEADER | DATA | METADATA | PLAYLISTINIT |
                      PLAYLISTHEADER | PLAYLISTDATA ) )

    {
      datamode = STOPREQD ;                           // Request STOP
    }
    else
    {
      strcpy ( reply, "Command not accepted!" ) ;     // Error reply
    }
  }
  else if ( argument == "resume" )                    // Request to resume?
  {
    if ( datamode == STOPPED )                        // Yes, are we stopped?
    {
      hostreq = true ;                                // Yes, request restart
    }
  }
  else if ( argument == "station" )                   // Station in the form address:port
  {
    if ( datamode & ( HEADER | DATA | METADATA | PLAYLISTINIT |
                      PLAYLISTHEADER | PLAYLISTDATA ) )
    {
      datamode = STOPREQD ;                           // Request STOP
    }
    host = value ;                                    // Save it for storage and selection later
    hostreq = true ;                                  // Force this station as new preset
    sprintf ( reply,
              "New preset station %s accepted",       // Format reply
              host.c_str() ) ;
  }
  else if ( argument == "xml" )
  {
    if ( datamode & ( HEADER | DATA | METADATA | PLAYLISTINIT |
                      PLAYLISTHEADER | PLAYLISTDATA ) )
    {
      datamode = STOPREQD ;                           // Request STOP
    }
    host = value ;                                    // Save it for storage and selection later
    xmlreq = true ;                                   // Run XML parsing process.
    sprintf ( reply,
              "New xml preset station %s accepted",   // Format reply
              host.c_str() ) ;
  }
  else if ( argument == "status" )                    // Status request
  {
    if ( datamode == STOPPED )
    {
      sprintf ( reply, "Player stopped" ) ;           // Format reply
    }
    else
    {
      sprintf ( reply, "%s - %s", icyname.c_str(),
                icystreamtitle.c_str() ) ;            // Streamtitle from metadata
    }
  }
  else if ( argument.startsWith ( "reset" ) )         // Reset request
  {
    resetreq = true ;                                 // Reset all
  }
  else if ( argument == "testfile" )                  // Testfile command?
  {
    testfilename = value ;                            // Yes, set file to test accordingly
  }
  else if ( argument == "test" )                      // Test command
  {
    sprintf ( reply, "Free memory is %d, ringbuf %d, stream %d",
              system_get_free_heap_size(), rcount, mp3client->available() ) ;
  }
  // Commands for bass/treble control
  else if ( argument.startsWith ( "tone" ) )          // Tone command
  {
    if ( argument.indexOf ( "ha" ) > 0 )              // High amplitue? (for treble)
    {
      ini_block.rtone[0] = ivalue ;                   // Yes, prepare to set ST_AMPLITUDE
    }
    if ( argument.indexOf ( "hf" ) > 0 )              // High frequency? (for treble)
    {
      ini_block.rtone[1] = ivalue ;                   // Yes, prepare to set ST_FREQLIMIT
    }
    if ( argument.indexOf ( "la" ) > 0 )              // Low amplitue? (for bass)
    {
      ini_block.rtone[2] = ivalue ;                   // Yes, prepare to set SB_AMPLITUDE
    }
    if ( argument.indexOf ( "lf" ) > 0 )              // High frequency? (for bass)
    {
      ini_block.rtone[3] = ivalue ;                   // Yes, prepare to set SB_FREQLIMIT
    }
    reqtone = true ;                                  // Set change request
    sprintf ( reply, "Parameter for bass/treble %s set to %d",
              argument.c_str(), ivalue ) ;
  }
  else if ( argument.startsWith ( "mqtt" ) )          // Parameter fo MQTT?
  {
    strcpy ( reply, "MQTT broker parameter changed. Save and restart to have effect" ) ;
    if ( argument.indexOf ( "broker" ) > 0 )          // Broker specified?
    {
      ini_block.mqttbroker = value.c_str() ;          // Yes, set broker accordingly
    }
    else if ( argument.indexOf ( "port" ) > 0 )       // Port specified?
    {
      ini_block.mqttport = ivalue ;                   // Yes, set port user accordingly
    }
    else if ( argument.indexOf ( "user" ) > 0 )       // User specified?
    {
      ini_block.mqttuser = value ;                    // Yes, set user accordingly
    }
    else if ( argument.indexOf ( "passwd" ) > 0 )     // Password specified?
    {
      ini_block.mqttpasswd = value.c_str() ;          // Yes, set broker password accordingly
    }
    else if ( argument.indexOf ( "pubtopic" ) > 0 )   // Publish topic specified?
    {
      ini_block.mqttpubtopic = value.c_str() ;        // Yes, set broker password accordingly
    }
    else if ( argument.indexOf ( "topic" ) > 0 )      // Topic specified?
    {
      ini_block.mqtttopic = value.c_str() ;           // Yes, set broker topic accordingly
    }
  }
  else if ( argument == "debug" )                     // debug on/off request?
  {
    DEBUG = ivalue ;                                  // Yes, set flag accordingly
  }
  else if ( argument == "analog" )                    // Show analog request?
  {
    sprintf ( reply, "Analog input = %d units",       // Read the analog input for test
              analogRead ( A0 ) ) ;
  }
  else if ( argument.startsWith ( "wifi" ) )          // WiFi SSID and passwd?
  {
    inx = value.indexOf ( "/" ) ;                     // Find separator between ssid and password
    // Was this the strongest SSID or the only acceptable?
    if ( num_an == 1 )
    {
      ini_block.ssid = value.substring ( 0, inx ) ;   // Only one.  Set as the strongest
    }
    if ( value.substring ( 0, inx ) == ini_block.ssid )
    {
      ini_block.passwd = value.substring ( inx + 1 ) ; // Yes, set password
    }
  }
  else if ( argument == "getnetworks" )               // List all WiFi networks?
  {
    sprintf ( reply, networks.c_str() ) ;             // Reply is SSIDs
  }
  else
  {
    sprintf ( reply, "%s called with illegal parameter: %s",
              NAME, argument.c_str() ) ;
  }
  return reply ;                                      // Return reply to the caller
}


//******************************************************************************************
//                             H A N D L E C M D                                           *
//******************************************************************************************
// Handling of the various commands from remote (case sensitive). All commands have the    *
// form "/?parameter[=value]".  Example: "/?volume=50".                                    *
// The startpage will be returned if no arguments are given.                               *
// Multiple parameters are ignored.  An extra parameter may be "version=<random number>"   *
// in order to prevent browsers like Edge and IE to use their cache.  This "version" is    *
// ignored.                                                                                *
// Example: "/?upvolume=5&version=0.9775479450590543"                                      *
// The save and the list commands are handled specially.                                   *
//******************************************************************************************
void handleCmd ( AsyncWebServerRequest* request )
{
  AsyncWebParameter* p ;                                // Points to parameter structure
  static String      argument ;                         // Next argument in command
  static String      value ;                            // Value of an argument
  const char*        reply ;                            // Reply to client
  //uint32_t         t ;                                // For time test
  int                params ;                           // Number of params
  static File        f ;                                // Handle for writing /radio.ini to SPIFFS

  //t = millis() ;                                      // Timestamp at start
  params = request->params() ;                          // Get number of arguments
  if ( params == 0 )                                    // Any arguments
  {
    if ( NetworkFound )
    {
      handleFSf ( request, String( "/index.html") ) ;   // No parameters, send the startpage
    }
    else
    {
      handleFSf ( request, String( "/config.html") ) ;  // Or the configuration page if in AP mode
    }
    return ;
  }
  p = request->getParam ( 0 ) ;                         // Get pointer to parameter structure
  argument = p->name() ;                                // Get the argument
  argument.toLowerCase() ;                              // Force to lower case
  value = p->value() ;                                  // Get the specified value
  // For the "save" command, the contents is the value of the next parameter
  if ( argument.startsWith ( "save" ) && ( params > 1 ) )
  {
    reply = "Error saving " INIFILENAME ;               // Default reply
    p = request->getParam ( 1 ) ;                       // Get pointer to next parameter structure
    if ( p->isPost() )                                  // Does it have a POST?
    {
      f = SPIFFS.open ( INIFILENAME, "w" ) ;            // Save to inifile
      if ( f )
      {
        f.print ( p->value() ) ;
        f.close() ;
        reply = dbgprint ( "%s saved", INIFILENAME ) ;
      }
    }
  }
  else if ( argument.startsWith ( "list" ) )            // List all presets?
  {
    dbgprint ( "list request from browser" ) ;
    request->send ( 200, "text/plain", presetlist ) ;   // Send the reply
    return ;
  }
  else
  {
    reply = analyzeCmd ( argument.c_str(),              // Analyze it
                         value.c_str() ) ;
  }
  request->send ( 200, "text/plain", reply ) ;          // Send the reply
  //t = millis() - t ;
  // If it takes too long to send a reply, we run into the "LmacRxBlk:1"-problem.
  // Reset the ESP8266.....
  //if ( t > 8000 )
  //{
  //  ESP.restart() ;                                   // Last resource
  //}
}
