//******************************************************************************************
//*  Esp_radio -- Webradio receiver for ESP8266, 1.8 color display and VS1053 MP3 module.  *
//*  With ESP8266 running at 80 MHz, it is capable of handling up to 256 kb bitrate.       *
//*  With ESP8266 running at 160 MHz, it is capable of handling up to 320 kb bitrate.      *
//******************************************************************************************
// ESP8266 libraries used:
//  - ESP8266WiFi
//  - SPI
//  - Adafruit_GFX
//  - TFT_ILI9163C
//  - ESPAsyncTCP
//  - ESPAsyncWebServer
//  - FS
//  - ArduinoOTA
//  - AsyncMqttClient
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
// Issue:
// Webserver produces error "LmacRxBlk:1" after some time.  After that it will work very slow.
// The program will reset the ESP8266 in such a case.  Now we have switched to async webserver,
// the problem still exists, but the program will not crash anymore.
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
// 07-05-2016, ES: Added preset selection in webserver
// 12-05-2016, ES: Added support for Ogg-encoder
// 13-05-2016, ES: Better Ogg detection
// 17-05-2016, ES: Analog input for commands, extra buttons if no TFT required.
// 26-05-2016, ES: Fixed BUTTON3 bug (no TFT)
// 27-05-2016, ES: Fixed restore station at restart
// 04-07-2016, ES: WiFi.disconnect clears old connection now (thanks to Juppit)
// 23-09-2016, ES: Added commands via MQTT and Serial input, Wifi set-up in AP mode
// 04-10-2016, ES: Configuration in .ini file. No more use of EEPROM and .pw files.
//
// Define the version number:
#define VERSION "07-oct-2016"
// TFT.  Define USETFT if required.
#define USETFT
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <SyncClient.h> Seems to loose some bytes, resulting in sync error metadata
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
#define	BLACK   0x0000
#define	BLUE    0xF800
#define	RED     0x001F
#define	GREEN   0x07E0
#define CYAN    GREEN | BLUE
#define MAGENTA RED | BLUE
#define YELLOW  RED | GREEN  
#define WHITE   0xFFFF
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
// Maximal length of the URL of a host
#define MAXHOSTSIZ 128
// Ringbuffer for smooth playing. 20000 bytes is 160 Kbits, about 1.5 seconds at 128kb bitrate.
#define RINGBFSIZ 10000
// Debug buffer size
#define DEBUG_BUFFER_SIZE 100
// Name of the ini file
#define INIFILENAME "/radio.ini"
// Access point name if connection to WiFi network fails.  Also the hostname for WiFi and OTA.
// Not that the password of an AP must be at least as long as 8 characters.
#define APNAME "Esp-radio"

//
//******************************************************************************************
// Forward declaration of various functions                                                *
//******************************************************************************************
void   displayinfo ( const char *str, int pos, uint16_t color ) ;
void   showstreamtitle() ;
void   handlebyte ( uint8_t b ) ;
void   handleFS ( AsyncWebServerRequest *request ) ;
void   handleCmd ( AsyncWebServerRequest *request )  ;
void   handleFileUpload ( AsyncWebServerRequest *request, String filename,
                          size_t index, uint8_t *data, size_t len, bool final ) ;
char*  dbgprint( const char* format, ... ) ;
char*  analyzeCmd ( const char* str ) ;
char*  analyzeCmd ( const char* par, const char* val ) ;
String chomp ( String str ) ;
void publishIP() ;

//
//******************************************************************************************
// Global data section.                                                                    *
//******************************************************************************************
// There is a block ini-data that contains some configuration.  This data can be saved in  *
// the SPIFFS file announcer.ini by the webinterface.  On restart the new data will be     *
// read from this file.  The file will not be saved automatically to prevent wear-out of   *
// the flash.  Items in ini_block can be changed by commands from webserver/MQTT/Serial.   *
//******************************************************************************************
struct ini_struct
{
  char           mqttbroker[80] ;                          // The name of the MQTT broker server
                                                           // Example: "mqtt.smallenburg.nl"
  uint16_t       mqttport ;                                // Port, default 1883
  char           mqttuser[16] ;                            // User for MQTT authentication
  char           mqttpasswd[16] ;                          // Password for MQTT authentication
  char           mqtttopic[16] ;                           // Topic to suscribe to
  char           mqttpubtopic[16] ;                        // Topic to pubtop (IP will be published)
  uint8_t        reqvol ;                                  // Requested volume
  uint8_t        rtone[4] ;                                // Requested bass/treble settings
  int8_t         newpreset ;                               // Requested preset
  String         ssid ;                                    // SSID of WiFi network to connect to
  String         passwd ;                                  // Password for WiFi network
} ;

enum datamode_t { INIT, HEADER, DATA, METADATA } ;         // State for datastream

// Global variables
int              DEBUG = 1 ;
ini_struct       ini_block ;                               // Holds configurable data
WiFiClient       mp3client ;                               // An instance of the mp3 client
AsyncWebServer   cmdserver ( 80 ) ;                        // Instance of embedded webserver
AsyncMqttClient  mqttclient ;                              // Client for MQTT subscriber
IPAddress        mqtt_server_IP ;                          // IP address of MQTT broker
char             cmd[130] ;                                // Command from MQTT or Serial
#if defined ( USETFT )
TFT_ILI9163C     tft = TFT_ILI9163C ( TFT_CS, TFT_DC ) ;
#endif
Ticker           tckr ;                                    // For timing 100 msec
uint32_t         totalcount = 0 ;                          // Counter mp3 data
datamode_t       datamode ;                                // State of datastream
int              metacount ;                               // Number of bytes in metadata
int              datacount ;                               // Counter databytes before metadata
char             metaline[200] ;                           // Readable line in metadata
char             streamtitle[150] ;                        // Streamtitle from metadata
int              bitrate ;                                 // Bitrate in kb/sec            
int              metaint = 0 ;                             // Number of databytes between metadata
int8_t           currentpreset = -1 ;                      // Preset station playing
char             host[MAXHOSTSIZ] ;                        // The hostname to connect to or file to play
bool             hostreq ;                                 // Request for new host
bool             stopreq = false ;                         // Request to stop playing
bool             playreq = false ;                         // Request for mp3 file to play
bool             playing = false ;                         // Playing active (for data guard)
char             sname[100] ;                              // Stationname
int              port ;                                    // Port number for host
int              delpreset = 0 ;                           // Preset to be deleted if nonzero
uint8_t          savvolume ;                               // Saved volume
bool             reqtone = false ;                         // new tone setting requested
uint8_t          savpreset ;                               // Saved preset station
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
uint8_t          num_an ;                                  // Number of acceptable networks in .ini file
char             testfilename[20] ;                        // File to test (SPIFFS speed)

//******************************************************************************************
// End of lobal data section.                                                              *
//******************************************************************************************
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
        yield() ;                               // Very short delay
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
  uint16_t value ;                                      // Value to send to SCI_VOL
  
  if ( vol != curvol )
  {
    curvol = vol ;                                      // Save for later use
    value = map ( vol, 0, 100, 0xFF, 0x00 ) ;           // 0..100% to one channel
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
VS1053 mp3 (  VS1053_CS, VS1053_DCS, VS1053_DREQ ) ;

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
  *(ringbuf+rbwindex) = b ;           // Put byte in ringbuffer
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
  return *(ringbuf+rbrindex) ;        // return the oldest byte
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
//                                  D B G P R I N T                                        *
//******************************************************************************************
// Send a line of info to serial output.  Works like vsprintf(), but checks the BEDUg flag.*
// Print only if DEBUG flag is true.  Always returns the the formatted string.             *
//******************************************************************************************
char* dbgprint ( const char* format, ... )
{
  static char sbuf[DEBUG_BUFFER_SIZE] ;               // For debug lines
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
// If totalcount has not been changed, there is a problem and a reset will be performed.   *
// Note that a "yield()" within this routine or in called functions will cause a crash!    *
//******************************************************************************************
void timer10sec()
{
  static uint32_t oldtotalcount = 7321 ;          // Needed foor change detection
  static uint8_t  morethanonce = 0 ;              // Counter for succesive fails
  static uint8_t  t600 = 0 ;                      // Counter for 10 minutes

  if ( playing )                                  // Test op continious play?
  {
    if ( totalcount == oldtotalcount )
    {
      // No data detected!
      dbgprint ( "No data input" ) ;
      if ( morethanonce > 10 )                    // Happened more than 10 times?
      {
        dbgprint ( "Going to restart..." ) ;
        ESP.restart() ;                           // Reset the CPU, probably no return
      }
      if ( morethanonce >= 1 )                    // Happened more than once?
      {
        ini_block.newpreset++ ;                   // Yes, try next channel
        dbgprint ( "Trying other station..." ) ;
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
      newdist = abs ( analogsw[i] - v ) ;          // Compute difference
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
void testfile ( char* fspec )
{
  String   path ;                                      // Full file spec
  File     tfile ;                                     // File containing mp3
  uint32_t len, savlen ;                               // File length
  uint32_t t0, t1 ;                                    // For time test
  uint32_t t_error = 0 ;                               // Number of slow reads
  
  t0 = millis() ;                                      // Timestamp at start
  t1 = t0 ;                                            // Prevent uninitialized value
  path = String ( "/" ) + String ( fspec ) ;           // Form full path
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
    }
    tfile.close() ;
  }
  // Show results for debug
  dbgprint ( "Read %s, length %d took %d seconds, %d slow reads",
             fspec, savlen, ( t1 - t0 ) / 1000, t_error ) ;
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
        dbgprint ( "Digital button 2 pushed" ) ;
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
        dbgprint ( "Digital button 1 pushed" ) ;
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
        dbgprint ( "Digital button 3 pushed" ) ;
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
        dbgprint ( "Analog button %d pushed, v = %d", anewval, v ) ;
        if ( anewval == 1 )                       // Button 1?
        {
          ini_block.newpreset = 0 ;               // Yes, goto first preset
        }
        else if ( anewval == 2 )                  // Button 2?
        {
          ini_block.newpreset = currentpreset+1 ; // Yes, goto next preset
        }
        else if ( anewval == 3 )                  // Button 3?
        {
          ini_block.newpreset = currentpreset-1 ; // Yes, goto previous preset
        }
      }
    }
  }
}


//******************************************************************************************
//                              D I S P L A Y I N F O                                      *
//******************************************************************************************
// Show a string on the LCD at a specified y-position in a specified color                 *
//******************************************************************************************
void displayinfo ( const char *str, int pos, uint16_t color )
{
#if defined ( USETFT )
  tft.fillRect ( 0, pos, 160, 40, BLACK ) ;   // Clear the space for new info
  tft.setTextColor ( color ) ;                // Set the requested color
  tft.setCursor ( 0, pos ) ;                  // Prepare to show the info
  tft.print ( str ) ;                         // Show the string
#endif
}


//******************************************************************************************
//                        S H O W S T R E A M T I T L E                                    *
//******************************************************************************************
// show artist and songtitle if present in metadata                                        *
//******************************************************************************************
void showstreamtitle ( char *ml )
{
  char*       p1 ;
  char*       p2 ;

  if ( strstr ( ml, "StreamTitle=" ) )
  {
    dbgprint ( "Streamtitle found, %d bytes", strlen ( ml ) ) ;
    dbgprint ( ml ) ;
    p1 = metaline + 12 ;                        // Begin of artist and title
    if ( ( p2 = strstr ( ml, ";" ) ) )          // Search for end of title
    {
      if ( *p1 == '\'' )                        // Surrounded by quotes?
      {
        p1++ ;
        p2-- ;
      }
      *p2 = '\0' ;                              // Strip the rest of the line
    }
    if ( *p1 == ' ' )                           // Leading space?
    {
      p1++ ;
    }
    // Save last part of string as streamtitle.  Protect against buffer overflow
    strncpy ( streamtitle, p1, sizeof ( streamtitle ) ) ;
    streamtitle[sizeof ( streamtitle ) - 1] = '\0' ;
  }
  else
  {
    return ;                                    // Metadata does not contain streamtitle
  }
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
  displayinfo ( streamtitle, 20, CYAN ) ;       // Show title at position 20
}


//******************************************************************************************
//                            C O N N E C T T O H O S T                                    *
//******************************************************************************************
// Connect to the Internet radio server specified by newpreset.                            *
//******************************************************************************************
void connecttohost()
{
  char*       p ;                                   // Pointer in hostname
  char*       pfs ;                                 // Pointer to formatted string
  String      extension ;                           // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
  
  dbgprint ( "Connect to new host %s", host ) ;
  if ( mp3client.connected() )
  {
    dbgprint ( "Stop client" ) ;                    // Stop conection to host
    mp3client.flush() ;
    mp3client.stop() ;
  }
  displayinfo ( "   ** Internet radio **", 0, WHITE ) ;
  port = 80 ;                                       // Default port
  p = strstr ( host, ":" ) ;                        // Search for separator
  if ( p )                                          // Portnumber available?
  {
    *p++ = '\0' ;                                   // Remove port from string and point to port
     port = atoi ( p ) ;                            // Get portnumber as integer
  }
  else
  {
    p = host ;                                      // No port number, reset pointer to begin of host
  }
  // After the portnumber or host there may be an extension
  extension = String ( "/" ) ;                      // Assume no extension
  p = strstr ( p, "/" ) ;                           // Search for begin of extension
  if ( p )                                          // Is there an extension?
  {
    extension = String ( p ) ;                      // Yes, change the default
    *p = '\0' ;                                     // Remove extension from host
    dbgprint ( "Slash in station" ) ;
  }
  pfs = dbgprint ( "Connect to preset %d, host %s on port %d, extension %s",
                   currentpreset, host, port, extension.c_str() ) ;
  displayinfo ( pfs, 60, YELLOW ) ;                 // Show info at position 60
  delay ( 2000 ) ;                                  // Show for some time
  mp3client.flush() ;
  if ( mp3client.connect ( host, port ) )
  {
    dbgprint ( "Connected to server" ) ;
    // This will send the request to the server. Request metadata.
    mp3client.print ( String ( "GET " ) +
                      extension +  
                      " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Icy-MetaData:1\r\n" +
                      "Connection: close\r\n\r\n");
  }
  datamode = INIT ;                                 // Start in metamode
  playing = true ;                                  // Allow data guard
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
  
  WiFi.disconnect() ;                                  // After restart the router could still keep the old connection
  WiFi.softAPdisconnect(true) ;
  WiFi.begin ( ini_block.ssid.c_str(),
               ini_block.passwd.c_str() ) ;            // Connect to selected SSID
  dbgprint ( "Try WiFi %s", ini_block.ssid.c_str() ) ; // Message to show during WiFi connect
  if (  WiFi.waitForConnectResult() != WL_CONNECTED )  // Try to connect
  {
    dbgprint ( "WiFi Failed!  Trying to setup AP with name %s and password %s.", APNAME, APNAME ) ;
    //WiFi.disconnect() ;                              // After restart the router could still keep the old connection
    //WiFi.softAPdisconnect(true) ;
    WiFi.softAP ( APNAME, APNAME ) ;                   // This ESP will be an AP
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
char* readhostfrominifile ( int8_t preset )
{
  String      path ;                                   // Full file spec as string
  File        inifile ;                                // File containing URL with mp3
  char        tkey[10] ;                               // Key as an array of chars
  String      line ;                                   // Input line from .ini file
  String      linelc ;                                 // Same, but lowercase
  int         inx ;                                    // Position within string
  static char newhost[MAXHOSTSIZ] ;                    // Found host name
  char*       res = NULL ;                             // Assume not found                                    

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
          line = chomp ( line ) ;                      // Remove garbage
          strncpy ( newhost, line.c_str(),             // Save result
                    sizeof(newhost) ) ; 
          res = newhost ;                              // Return new host
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
  char     ip[20] ;                             // Hold IP as string
  
  if ( strlen ( ini_block.mqttpubtopic ) )      // Topic to publish?
  {
    // Publish IP-adress.  qos=1, retain=true
    sprintf ( ip, "%d.%d.%d.%d",
              WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] ) ;
    mqttclient.publish ( ini_block.mqttpubtopic, 1, true, ip ) ;
    dbgprint ( "Publishing IP %s to topic %s",
               ip, ini_block.mqttpubtopic ) ;
  }
}


//******************************************************************************************
//                            O N M Q T T C O N N E C T                                    * 
//******************************************************************************************
// Will be called on connection to the broker.  Subscribe to our topic and publish a topic.*
//******************************************************************************************
void onMqttConnect()
{
  uint16_t packetIdSub ;
  
  dbgprint ( "MQTT Connected to the broker %s", ini_block.mqttbroker ) ;
  packetIdSub = mqttclient.subscribe ( ini_block.mqtttopic, 2 ) ;
  dbgprint ( "Subscribing to %s at QoS 2, packetId = %d ",
             ini_block.mqtttopic,
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
  dbgprint ( "MQTT Disconnected from the broker, reason %d,reconnecting...",
             reason ) ;
  mqttclient.connect() ;
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
    Serial.write ( c ) ;                         // Echo
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
void  mk_lsan()
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
    dbgprint ( "%-32s - %6d",                          // Show name and size
               filename.c_str(), f.size() ) ;
  }
  mk_lsan() ;                                          // Make al list of acceptable networks in ini file.
  listNetworks() ;                                     // Search for WiFi networks
  readinifile() ;                                      // Read .ini file
  WiFi.persistent ( false ) ;                          // Do not save SSID and password
  WiFi.disconnect() ;                                  // After restart the router could still keep the old connection
  WiFi.mode ( WIFI_STA ) ;                             // This ESP is a station
  wifi_station_set_hostname ( (char*)"ESP-radio" ) ; 
  SPI.begin() ;                                        // Init SPI bus
  // Print some memory and sketch info
  dbgprint ( "Starting ESP Version " VERSION "...  Free memory %d",
             system_get_free_heap_size() ) ;
  dbgprint ( "Sketch size %d, free size %d",
              ESP.getSketchSize(),
              ESP.getFreeSketchSpace() ) ;
  pinMode ( BUTTON2, INPUT_PULLUP ) ;                  // Input for control button 2
  mp3.begin() ;                                        // Initialize VS1053 player
  # if defined ( USETFT )
  tft.begin() ;                                        // Init TFT interface
  tft.fillRect ( 0, 0, 160, 128, BLACK ) ;             // Clear screen does not work when rotated
  tft.setRotation ( 3 ) ;                              // Use landscape format
  tft.clearScreen() ;                                  // Clear screen
  tft.setTextSize ( 1 ) ;                              // Small character font
  tft.setTextColor ( WHITE ) ;  
  tft.println ( "Starting" ) ;
  #else
  pinMode ( BUTTON1, INPUT_PULLUP ) ;                  // Input for control button 1
  pinMode ( BUTTON3, INPUT_PULLUP ) ;                  // Input for control button 3
  #endif
  delay(10);
  streamtitle[0] = '\0' ;                              // No title yet
  hostreq = false ;                                    // No host yet
  analogrest = ( analogRead ( A0 ) + asw1 ) / 2  ;     // Assumed inactive analog input
  tckr.attach ( 0.100, timer100 ) ;                    // Every 100 msec
  dbgprint ( "Selected network: %-25s", ini_block.ssid.c_str() ) ;
  NetworkFound = connectwifi() ;                       // Connect to WiFi network
  dbgprint ( "Start server for commands" ) ;
  cmdserver.on ( "/", handleCmd ) ;                    // Handle startpage
  cmdserver.onNotFound ( handleFS ) ;                  // Handle file from FS
  cmdserver.onFileUpload ( handleFileUpload ) ;        // Handle file uploads
  cmdserver.begin() ;
  if ( NetworkFound )                                  // OTA and MQTT only if Wifi network found
  {
    ArduinoOTA.setHostname ( (char*)APNAME ) ;         // Set the hostname
    ArduinoOTA.onStart ( otastart ) ;
    ArduinoOTA.begin() ;                               // Allow update over the air
    if ( strlen ( ini_block.mqttbroker ) )             // Broker specified?
    {
      // Initialize the MQTT client
      WiFi.hostByName ( ini_block.mqttbroker,
                        mqtt_server_IP ) ;             // Lookup IP of MQTT server 
      mqttclient.onConnect ( onMqttConnect ) ;
      mqttclient.onDisconnect ( onMqttDisconnect ) ;
      mqttclient.onSubscribe ( onMqttSubscribe ) ;
      mqttclient.onUnsubscribe ( onMqttUnsubscribe ) ;
      mqttclient.onMessage ( onMqttMessage ) ;
      mqttclient.onPublish ( onMqttPublish ) ;
      mqttclient.setServer ( mqtt_server_IP,           // Specify the broker
                             ini_block.mqttport ) ;    // And the port
      mqttclient.setCredentials ( ini_block.mqttuser,
                                  ini_block.mqttpasswd ) ;
      mqttclient.setClientId ( "Esp-radio" ) ;
      dbgprint ( "Connecting to MQTT %s, port %d, user %s, password %s...",
                 ini_block.mqttbroker,
                 ini_block.mqttport,
                 ini_block.mqttuser,
                 ini_block.mqttpasswd ) ;
      mqttclient.connect();
    }
  }

  delay ( 1000 ) ;                                     // Show IP for a wile
  ArduinoOTA.setHostname ( "ESP-radio" ) ;             // Set the hostname
  ArduinoOTA.onStart ( otastart ) ;
  ArduinoOTA.begin() ;                                 // Allow update over the air
  analogrest = ( analogRead ( A0 ) + asw1 ) / 2  ;     // Assumed inactive analog input
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
  char*  p ;                                          // Temporary pointer to string 
  
  // Try to keep the ringbuffer filled up by adding as much bytes as possible 
  while ( ringspace() && mp3client.available() )
  {
    putring ( mp3client.read() ) ;                    // Yes, save one byte in ringbuffer
  }
  yield() ;
  while ( mp3.data_request() && ringavail() )         // Try to keep VS1053 filled
  {
    handlebyte ( getring() ) ;                        // Yes, handle it
  }
  if ( stopreq )                                      // Stop requested?
  {
    stopreq = false ;                                 // Yes, stop song
    playing = false ;                                 // No more guarding
    mp3client.flush() ;                               // Flush stream client
    mp3client.stop() ;                                // Stop stream client
    mp3.setVolume ( 0 ) ;                             // Mute
    mp3.stopSong() ;                                  // Stop playing
    emptyring() ;                                     // Empty the ringbuffer
  }
  if ( ini_block.newpreset != currentpreset )         // New station requested?
  {
    mp3.setVolume ( 0 ) ;                             // Mute
    mp3.stopSong() ;                                  // Stop playing
    emptyring() ;                                     // Empty the ringbuffer
    dbgprint ( "New preset requested = %d",
               ini_block.newpreset ) ;
    p = readhostfrominifile ( ini_block.newpreset ) ; // Lookup preset in ini-file
    if ( p )                                          // Preset in ini-file?
    {
      dbgprint ( "Preset %d found in .ini file",      // Yes
                 ini_block.newpreset ) ;
      strcpy ( host, p ) ;                            // Save it for storage and selection later 
      hostreq = true ;                                // Force this station as new preset
    }
    else
    {
      // This preset is not available, return to preset 0, will be handled in next loop()
      ini_block.newpreset = 0 ;                       // Wrap to first station
    }
  }
  if ( hostreq )
  {
    currentpreset = ini_block.newpreset ;             // Remember current preset
    dbgprint ( "Remember preset %d", currentpreset ) ;
    connecttohost() ;                                 // Switch to new host
    hostreq = false ;
  }
  mp3.setVolume ( ini_block.reqvol ) ;                // Set to requested volume
  if ( reqtone )                                      // Request to change tone?
  {
    reqtone = false ;
    mp3.setTone ( ini_block.rtone ) ;                 // Set SCI_BASS to requested value
  }
  if ( resetreq )                                     // Reset requested?
  {
    delay ( 1000 ) ;                                  // Yes, wait some time
    ESP.restart() ;                                   // Reboot
  }
  if ( muteflag )
  {
    mp3.setVolume ( 0 ) ;                             // Mute
  }
  else
  {
    mp3.setVolume ( ini_block.reqvol ) ;              // Unmute
  }
  if ( *testfilename )                                // File to test?
  {
    testfile ( testfilename ) ;                       // Yes, do the test
    *testfilename = '\0' ;                            // Clear test request
  }
  scanserial() ;                                      // Handle serial input
  ArduinoOTA.handle() ;                               // Check for OTA
}


//******************************************************************************************
//                           H A N D L E B Y T E                                           *
//******************************************************************************************
// Handle the next byte of data from server.                                               *
// This byte will be send to the VS1053 most of the time.                                  *
// Note that the buffer the data chunk must start at an address that is a muttiple of 4.   *
//******************************************************************************************
void handlebyte ( uint8_t b )
{
  static uint16_t  metaindex ;                          // Index in metaline
  static bool      firstmetabyte ;                      // True if first metabyte (counter) 
  static int       LFcount ;                            // Detection of end of header
  static __attribute__((aligned(4))) uint8_t buf[32] ;  // Buffer for chunk
  static int       chunkcount = 0 ;                     // Data in chunk
  static bool      firstchunk = true ;                  // First chunk as input
  char*            p ;                                  // Pointer in metaline
  int              i ;                                  // Loop control

  
  if ( datamode == INIT )                              // Initialize for header receive
  {
    metaint = 0 ;                                      // No metaint found
    LFcount = 0 ;                                      // For detection end of header
    bitrate = 0 ;                                      // Bitrate still unknown
    metaindex = 0 ;                                    // Prepare for new line
    datamode = HEADER ;                                // Handle header
    totalcount = 0 ;                                   // Reset totalcount
  }
  if ( datamode == DATA )                              // Handle next byte of MP3/Ogg data
  {
    buf[chunkcount++] = b ;                            // Save byte in cunkbuffer
    if ( chunkcount == sizeof(buf) )                   // Buffer full?
    {
      if ( firstchunk )
      {
        firstchunk = false ;
        dbgprint ( "First chunk:" ) ;                  // Header for printout of first chunk
        for ( i = 0 ; i < 32 ; i += 8 )                // Print 4 lines
        {
          dbgprint ( "%02X %02X %02X %02X %02X %02X %02X %02X",
                     buf[i],   buf[i+1], buf[i+2], buf[i+3],
                     buf[i+4], buf[i+5], buf[i+6], buf[i+7] ) ;
        }
      }
      mp3.playChunk ( buf, chunkcount ) ;              // Yes, send to player
      chunkcount = 0 ;                                 // Reset count
    }
    totalcount++ ;                                     // Count number of bytes, ignore overflow
    if ( metaint != 0 )                                // No METADATA on Ogg streams
    {
      if ( --datacount == 0 )                          // End of datablock?
      {
        if ( chunkcount )                              // Yes, still data in buffer?
        {
          mp3.playChunk ( buf, chunkcount ) ;          // Yes, send to player
          chunkcount = 0 ;                             // Reset count
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
      metaline[metaindex] = '\0' ;                     // Mark end of string
      metaindex = 0 ;                                  // Reset for next line
      dbgprint ( metaline ) ;                          // Show it
      if ( ( p = strstr ( metaline, "icy-br:" ) ) )
      {
        bitrate = atoi ( p + 7 ) ;                     // Found bitrate tag, read the bitrate
        if ( bitrate == 0 )                            // For Ogg br is like "Quality 2"
        {
          bitrate = 87 ;                               // Dummy bitrate
        }
      }
      else if ( (  p = strstr ( metaline, "icy-metaint:" ) ) )
      {
        metaint = atoi ( p + 12 ) ;                    // Found metaint tag, read the value
      }
      else if ( ( p = strstr ( metaline, "icy-name:" ) ) )
      {
        strncpy ( sname, p + 9, sizeof ( sname ) ) ;   // Found station name, save it, prevent overflow
        sname[sizeof(sname)-1] = '\0' ;
        displayinfo ( sname, 60, YELLOW ) ;            // Show title at position 60
      }
      if ( ( LFcount == 2 ) && ( bitrate != 0 ) )
      {
        dbgprint ( "Switch to DATA" ) ;
        datamode = DATA ;                              // Expecting data now
        datacount = metaint ;                          // Number of bytes before first metadata
        chunkcount = 0 ;                               // Reset chunkcount
        mp3.startSong() ;                              // Start a new song
      }
    }
    else
    {
      metaline[metaindex] = (char)b ;                  // Normal character, put new char in metaline
      if ( metaindex < ( sizeof(metaline) - 2 ) )      // Prevent buffer overflow
      {
        metaindex++ ;
      }
      LFcount = 0 ;                                    // Reset double CRLF detection
    }
    return ;
  }
  if ( datamode == METADATA )                          // Handle next bye of metadata
  {
    if ( firstmetabyte )                               // First byte of metadata?
    {
      firstmetabyte = false ;                          // Not the first anymore
      metacount = b * 16 + 1 ;                         // New count for metadata including length byte
      metaindex = 0 ;                                  // Place to store metadata
      if ( metacount > 1 )
      {
        dbgprint ( "Metadata block %d bytes",
                   metacount-1 ) ;                     // Most of the time there are zero bytes of metadata
      }
    }
    else
    {
      metaline[metaindex] = (char)b ;                 // Normal character, put new char in metaline
      if ( metaindex < ( sizeof(metaline) - 2 ) )     // Prevent buffer overflow
      {
        metaindex++ ;
      }
    }
    if ( --metacount == 0 )                         
    {
      if ( metaindex )                                // Any info present?
      {
        metaline[metaindex] = '\0' ;
        // metaline contains artist and song name.  For example:
        // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
        // Sometimes it is just other info like:
        // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
        // Isolate the StreamTitle, remove leading and trailing quotes if present.
        showstreamtitle ( metaline ) ;                // Show artist and title if present in metadata 
      }
      datacount = metaint ;                           // Reset data count
      chunkcount = 0 ;                                // Reset chunkcount
      datamode = DATA ;                               // Expecting data
    }
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
  else if ( filename.endsWith ( ".zip"  ) ) return "application/x-zip" ;
  else if ( filename.endsWith ( ".gz"   ) ) return "application/x-gzip" ;
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
    t = 0 ;                                           // Force first print
    totallength = 0 ;                                 // Total file lengt still zero
    lastindex = 0 ;                                   // Prepare test
  }
  t1 = millis() ;                                     // Current timestamp
  if ( ( ( t1 - t ) > 1000 ) ||                       // One second passed?
       final ||                                       // or final chunk?
       ( len != 1460 ) )                              // or strange length
  {
    // Yes, print progress
    dbgprint ( "File upload %s, len %d, index %d",
               filename.c_str(), len, index ) ;
    t = t1 ;
  }
  if ( len )                                          // Something to write?
  {
    if ( index != lastindex )                         // New chunk?
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
//                                H A N D L E F S                                          *
//******************************************************************************************
// Handling of requesting files from the SPIFFS. Example: /favicon.ico                     *
//******************************************************************************************
void handleFS ( AsyncWebServerRequest *request )
{
  String fnam ;                                         // Requested file
  String ct ;                                           // Content type

  fnam = request->url() ;
  dbgprint ( "onFileRequest received %s", fnam.c_str() ) ;
  ct = getContentType ( fnam ) ;                        // Get content type
  if ( ct == "" )                                       // Empty is illegal
  {
    request->send ( 404, "text/plain", "File not found" ) ;  
  }
  else
  {
    request->send ( SPIFFS, fnam, ct ) ;                // Okay, send the file
  }
}


//******************************************************************************************
//                             G E T P R E S E T S                                         *
//******************************************************************************************
// Make a list of all preset stations and return this to the client.                       *
//******************************************************************************************
void getpresets ( AsyncWebServerRequest *request )
{
  String              path ;                           // Full file spec as string
  File                inifile ;                        // File containing URL with mp3
  String              line ;                           // Input line from .ini file
  String              linelc ;                         // Same, but lowercase
  int                 inx ;                            // Position of search char in line
  int                 i ;                              // Loop control
  AsyncResponseStream *response ;                      // Response to client
  
  response = request->beginResponseStream ( "text/plain" ) ;
  path = String ( INIFILENAME ) ;                      // Form full path
  inifile = SPIFFS.open ( path, "r" ) ;                // Open the file
  if ( inifile )
  {
    while ( inifile.available() )
    {
      line = inifile.readStringUntil ( '\n' ) ;        // Read next line
      linelc = line ;                                  // Copy for lowercase
      linelc.toLowerCase() ;                           // Set to lowercase
      if ( linelc.startsWith ( "preset_" ) )           // Found the key?
      {
        i = linelc.substring(7,9).toInt() ;            // Get index 00..99
        inx = line.indexOf ( "#" ) ;                   // Get position of "#"
        if ( inx > 0 )                                 // Equal sign present?
        {
          line.remove ( 0, inx + 1 ) ;                 // Yes, remove non-comment part
        }
        else
        {
          inx = line.indexOf ( "=" ) ;                 // Get position of "="
          line.remove ( 0, inx + 1 ) ;                 // Yes, remove first part of line
        }
        line = chomp ( line ) ;                        // Remove garbage
        response->printf ( "%02d%s|", i,
                           line.c_str() ) ;            // 2 digits plus description
      }
    }
    inifile.close() ;                                  // Close the file
  }
  request->send ( response ) ;
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
//   preset     = 12                        // Select start preset to connect to *)        *
//   preset_00  = <mp3 stream>              // Specify station for a preset 00-99 *)       *
//   volume     = 95                        // Percentage between 0 and 100                *
//   upvolume   = 2                         // Add percentage to current volume            *
//   downvolume = 2                         // Subtract percentage from current volume     *
//   toneha     = <0..15>                   // Setting treble gain                         *
//   tonehf     = <0..15>                   // Setting treble frequency                    *
//   tonela     = <0..15>                   // Setting bass gain                           *
//   tonelf     = <0..15>                   // Setting treble frequency                    *
//   station    = <mp3 stream>              // Select new station (will not be saved)      *
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
//  Commands marked with "*)" are sensible in ini-file only                                *
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
  ivalue = abs ( value.toInt() ) ;                    // Also as an integer
  relative = argument.indexOf ( "up" ) == 0 ;         // + relative setting?
  if ( argument.indexOf ( "down" ) == 0 )             // - relative setting?
  {
    relative = true ;                                 // It's relative
    ivalue = - ivalue ;                               // But with negative value
  }
  dbgprint ( "Command: %s with parameter %s",
             argument.c_str(), value.c_str() ) ;
  if ( argument.indexOf ( "volume" ) >= 0 )           // Volume setting?
  {
    // Volume may be of the form "upvolume", "downvolume" or "volume" for relative or absolute setting
    oldvol = mp3.getVolume() ;                        // Get current volume
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
        ini_block.newpreset = ivalue ;                // Otherwise set station
      }
      dbgprint ( "Preset set to %d", ini_block.newpreset ) ;
    }
  }
  else if ( argument =="stop" )                       // Stop requested
  {
    stopreq = true ;                                  // Force stop playing
  }
  else if ( argument =="station" )                    // Station in the form address:port
  {
    strcpy ( host, value.c_str() ) ;                  // Save it for storage and selection later 
    hostreq = true ;                                  // Force this station as new preset
    sprintf ( reply,
              "New preset station %s accepted",       // Format reply
              host ) ;
  }
  else if ( argument =="play" )                       // Play standalone mp3 file requested?
  {
    strcpy ( host, value.c_str() ) ;                  // Save it for storage and selection later 
    playreq = true ;                                  // Force this mp3 to be played
    sprintf ( reply,
              "Mp3-file to play %s accepted",         // Format reply
              host ) ;
  }
  else if ( argument == "status" )                    // Status request
  {
    sprintf ( reply, "Playing preset %d - %s",        // Format reply
              currentpreset, host ) ;
  }
  else if ( argument.startsWith ( "reset" ) )         // Reset request
  {
    resetreq = true ;                                 // Reset all
  }
  else if ( argument == "testfile" )                  // Testfile command?
  {
    strncpy ( testfilename, value.c_str(),
                sizeof(testfilename) ) ;              // Yes, set file to test accordingly
  }
  else if ( argument == "test" )                      // Test command
  {
    sprintf ( reply, "Free memory is %d, ringbuf %d, stream %d",
              system_get_free_heap_size(), rcount, mp3client.available() ) ;
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
      strncpy ( ini_block.mqttbroker, value.c_str(),
                sizeof(ini_block.mqttbroker) ) ;      // Yes, set broker accordingly
    }
    else if ( argument.indexOf ( "port" ) > 0 )       // Port specified?
    {
      ini_block.mqttport = ivalue ;                   // Yes, set port user accordingly
    }
    else if ( argument.indexOf ( "user" ) > 0 )       // User specified?
    {
      strncpy ( ini_block.mqttuser, value.c_str(),
                sizeof(ini_block.mqttuser) ) ;        // Yes, set user user accordingly
    }
    else if ( argument.indexOf ( "passwd" ) > 0 )     // Password specified?
    {
      strncpy ( ini_block.mqttpasswd, value.c_str(),
                sizeof(ini_block.mqttpasswd) ) ;      // Yes, set broker password accordingly
    }
    else if ( argument.indexOf ( "pubtopic" ) > 0 )   // Publish topic specified?
    {
      strncpy ( ini_block.mqttpubtopic, value.c_str(),
                sizeof(ini_block.mqttpubtopic) ) ;    // Yes, set broker password accordingly
    }
    else if ( argument.indexOf ( "topic" ) > 0 )      // Topic specified?
    {
      strncpy ( ini_block.mqtttopic, value.c_str(),
                sizeof(ini_block.mqtttopic) ) ;       // Yes, set broker password accordingly
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
      ini_block.passwd = value.substring ( inx+1 ) ;  // Yes, set password
    }
  }
  else if ( argument == "getnetworks" )               // List all WiFi networks?
  {
    sprintf ( reply, networks.c_str() ) ;             // Reply is SSIDs
  }
  else
  {
    sprintf ( reply, "ESP-radio called with illegal parameter: %s",
              argument.c_str() ) ;
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
void handleCmd ( AsyncWebServerRequest *request )
{
  AsyncWebParameter* p ;                              // Points to parameter structure
  String             argument ;                       // Next argument in command
  String             value ;                          // Value of an argument
  const char*        reply ;                          // Reply to client
  uint32_t           t ;                              // For time test
  int                params ;                         // Number of params
  File               f ;                              // Handle for writing /announcer.ini to SPIFFS
  
  t = millis() ;                                      // Timestamp at start
  params = request->params() ;                        // Get number of arguments
  if ( params == 0 )                                  // Any arguments
  {
    if ( NetworkFound )
    {
      request->send ( SPIFFS, "/index.html",          // No parameters, send the startpage
                      "text/html" ) ;
    }
    else
    {
      request->send ( SPIFFS, "/config.html",        // Or the configuration page if in AP mode
                      "text/html" ) ;
    }
    return ;
  }
  p = request->getParam ( 0 ) ;                       // Get pointer to parameter structure
  argument = p->name() ;                              // Get the argument
  argument.toLowerCase() ;                            // Force to lower case
  value = p->value() ;                                // Get the specified value
    // For the "save" command, the contents is the value of the next parameter
  if ( argument.startsWith ( "save" ) && ( params > 1 ) )
  {
    reply = "Error saving " INIFILENAME ;             // Default reply
    p = request->getParam ( 1 ) ;                     // Get pointer to next parameter structure
    if ( p->isPost() )                                // Does it have a POST?
    {
      f = SPIFFS.open ( INIFILENAME, "w" ) ;          // Save to inifile
      if ( f )
      {
        f.print ( p->value() ) ;
        f.close() ;
        reply = dbgprint ( "%s saved", INIFILENAME ) ;
      }
    }
  }
  else if ( argument.startsWith ( "list" ) )          // List all presets?
  {
    dbgprint ( "list request from browser" ) ;
    getpresets ( request ) ;                          // Reply with station presets
    return ;
  }
  else
  {
    reply = analyzeCmd ( argument.c_str(),            // Analyze it 
                         value.c_str() ) ;
  }
  request->send ( 200, "text/plain", reply ) ;        // Send the reply
  t = millis() - t ;
  // If it takes too long to send a reply, we run into the "LmacRxBlk:1"-problem.
  // Reset the ESP8266..... 
  if ( t > 8000 )
  {
    ESP.restart() ;                                   // Last resource
  }
}

