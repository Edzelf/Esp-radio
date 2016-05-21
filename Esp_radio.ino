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
//  - EEPROM
//  - ESPAsyncTCP
//  - ESPAsyncWebServer
//  - FS
//  - ArduinoOTA
// A library for the VS1053 (for ESP8266) is not available (or not easy to find).  Therefore
// a class for this module is derived from the maniacbug library and integrated in this sketch.
//
// See http://www.internet-radio.com for suitable stations.  Add the stations of your choice
// to the table "hostlist" in the global data secting further on.  This will be written to
// EEPROM and can be modified through remote access through the web server.
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
// Pushing the input button causes the player to select the next station in the hostlist (EEPROM).
//
// The display used is a Chinese 1.8 color TFT module 128 x 160 pixels.  The TFT_ILI9163C.h
// file has been changed to reflect this particular module.  TFT_ILI9163C.cpp has been
// changed to use the full screenwidth if rotated to mode "3".  Now there is room for 26
// characters per line and 16 lines.  Software will work without installing the display.
// If no TFT is used, you may use GPIO2 and GPIO15 as control buttons.  See definition of "USETFT" below.
// Switches are than programmed as:
// GPIO2 : "Goto station 1"
// GPIO0 : "Next station"
// GPIO15: "Previous station".  Note that GPIO is pulled low for NodeMCU modules, use 10 kOhm for pull-up.
// Set these values to 2000 if not used or tie analog input to ground.

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
// VCC 5 V  -        -              -                   pin 9 5V             Power supply
// RST      -        -              pin 1 (RST)         pin 3 RESET          Reset circuit
//
// The reset circuit is a circuit with 2 diodes to GPIO5 and GPIO16 and a resistor to ground
// (wired OR gate) because there was not a free GPIO output available for this function.
// This circuit is included in the documentation.
// Issue:
// Webserver produces error "LmacRxBlk:1" after some time.  After that it will work verry slow.
// The program will reset the ESP8266 in such a case.  Now we have switched to async webserver,
// the problem still exists, but the program will not crash anymore.
//
// 31-03-2016, ES: First set-up.
// 01-04-2016, ES: Detect missing VS1053 at start-up.
// 05-04-2016, ES: Added commands through http server on port 80.
// 06-04-2016, ES: Added list of stations in EEPROM.
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
//
// TFT.  Define USETFT if required.
#define USETFT
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#if defined ( USETFT )
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#endif
#include <Ticker.h>
#include <stdio.h>
#include <string.h>
#include <EEPROM.h>
#include <FS.h>
#include <ArduinoOTA.h>

extern "C"
{
  #include "user_interface.h"
}

#define DEBUG_ESP_RADIO 1
#define DEBUG_BUFFER_SIZE 128

// Definitions for 3 control switches on analog input
// You can test the analog input values by holding down the switch and select /?analog=1
// in the web interface. See schematics in the documentation.
// Switches are programmed as "Goto station 1", "Next station" and "Previous station" respectively.
// Set these values to 2000 if not used or tie analog input to ground.
#define NUMANA  3
#define asw1    252
#define asw2    334
#define asw3    499
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
// Maximal number of presets in EEPROM and size of an entry
#define EEPROM_SIZE 4096         // Size of EEPROM to use for saving preset stations
#define EESIZ 128                // Maximal number of presets in EEPROM
#define EENUM EEPROM_SIZE/EESIZ  // Size of one preset entry in EEPROM

// Ringbuffer for smooth playing. 20000 bytes is 160 Kbits, about 1.5 seconds at 128kb bitrate.
#define RINGBFSIZ 20000

//******************************************************************************************
// Forward declaration of methods                                                                  *
//******************************************************************************************
void displayinfo ( const char *str, int pos, uint16_t color );
int find_eeprom_station ( const char *search_entry );
void put_empty_eeprom_station ( int index );
int find_free_eeprom_entry();
char* get_eeprom_station ( int index );
void put_eeprom_station ( int index, const char *entry );
void showstreamtitle();
void handlebyte ( uint8_t b );
void handleFS ( AsyncWebServerRequest *request );
void handleCmd ( AsyncWebServerRequest *request );
void dbgprint( const char* format, ... );

//******************************************************************************************
// Global data section.                                                                    *
//******************************************************************************************
enum datamode_t { INIT, HEADER, DATA, METADATA } ;         // State for datastream
// Global variables
String           ssid ;                                    // SSID of selected WiFi network
int              DEBUG = 1 ;
WiFiClient       mp3client ;                               // An instance of the mp3 client
AsyncWebServer   cmdserver ( 80 ) ;                        // Instance of embedded webserver
String           cmd ;                                     // Command from remote
#if defined ( USETFT )
TFT_ILI9163C     tft = TFT_ILI9163C ( TFT_CS, TFT_DC ) ;
#endif
Ticker           tckr ;                                    // For timing 10 sec
uint32_t         totalcount = 0 ;                          // Counter mp3 data
char             sbuf[DEBUG_BUFFER_SIZE] ;                 // For debug lines
datamode_t       datamode ;                                // State of datastream
int              metacount ;                               // Number of bytes in metadata
int              datacount ;                               // Counter databytes before metadata
char             metaline[200] ;                           // Readable line in metadata
char             streamtitle[150] ;                        // Streamtitle from metadata
int              bitrate ;                                 // Bitrate in kb/sec            
int              metaint = 0 ;                             // Number of databytes between metadata
int              currentpreset = 1 ;                       // Preset station to play (index in hostlist (EEPROM))
int              newpreset = 1 ;                           // Requested preset
char             host[EESIZ] ;                             // The hostname
char             sname[100] ;                              // Stationname
int              port ;                                    // Port number for host
char             newstation[EESIZ] ;                       // Station:port from remote
int              delpreset = 0 ;                           // Preset to be deleted if nonzero
uint8_t          reqvol = 80 ;                             // Requested volume
uint8_t          savvolume ;                               // Saved volume
uint8_t          rtone[4]    ;                             // Requested bass/treble settings
bool             reqtone = false ;                         // new tone setting requested
uint8_t          savpreset ;                               // Saved preset station
bool             savreq = false ;                          // Reqwuest to save settings      
char             currentstat[EESIZ] ;                      // Current station:port
uint8_t*         ringbuf ;                                 // Ringbuffer for VS1053
uint16_t         rbwindex = 0 ;                            // Fill pointer in ringbuffer
uint16_t         rbrindex = RINGBFSIZ - 1 ;                // Emptypointer in ringbuffer
uint16_t         rcount = 0 ;                              // Number of bytes in ringbuffer
uint16_t         analogsw[NUMANA] = { asw1, asw2, asw3 } ; // 3 levels of analog input
uint16_t         analogrest ;                              // Rest value of analog input

//
// List of initial preset stations.
// This will be copied to EEPROM if EEPROM is empty.  The first entry [0] is reserved
// for detection of a not yet filled EEPROM.  The last 2 bytes for saved volume and preset.
// In EEPROM, every entry takes EESIZ bytes.
const char*      hostlist[] = {
                     "Stations from remote control",  // Reserved entry
                     "109.206.96.34:8100",            //  1 - NAXI LOVE RADIO, Belgrade, Serbia 128-kbps
                     "us1.internet-radio.com:8180",   //  2 - Easy Hits Florida 128-kbps
                     "us2.internet-radio.com:8050",   //  3 - CLASSIC ROCK MIA WWW.SHERADIO.COM
                     "us1.internet-radio.com:15919",  //  4 - Magic Oldies Florida
                     "us2.internet-radio.com:8132",   //  5 - Magic 60s Florida 60s Top 40 Classic Rock
                     "us1.internet-radio.com:8105",   //  6 - Classic Rock Florida - SHE Radio
                     "205.164.36.153:80",             //  7 - BOM PSYTRANCE (1.FM TM)  64-kbps
                     "205.164.62.15:10032",           //  8 - 1.FM - GAIA, 64-kbps
                     "skonto.ls.lv:8002/mp3",         //  9 - Skonto 128-kpbs
                     "85.17.121.216:8468",            // 10 - RADIO LEHOVO 971 GREECE, 64-kbps
                     "85.17.121.103:8800",            // 11 - STAR FM 88.8 Corfu Greece, 64-kbps
                     "85.17.122.39:8530",             // 12 - stylfm.gr laiko, 64-kbps
                     "94.23.66.155:8106",             // 13 - *ILR CHILL & GROOVE* 64-kbps
                     "205.164.62.22:7012",            // 14 - 1.FM - ABSOLUTE TRANCE (EURO) RADIO 64-kbps
                     NULL } ;

//******************************************************************************************
// End of global data section.                                                              *
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
  char          lsbuf[60] ;                     // For debugging
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
      sprintf ( lsbuf, "VS1053 error retry SB:%04X R1:%04X R2:%04X", i, r1, r2 ) ;
      dbgprint ( lsbuf ) ;
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
  // Init SPI in slow mode ( 2 MHz )
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
  sprintf ( lsbuf, "endFillByte is %X", endFillByte ) ;
  dbgprint ( lsbuf ) ;
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
      sprintf ( lsbuf, "Song stopped correctly after %d msec",
                i * 10 ) ;
      dbgprint ( lsbuf ) ;
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
    sprintf ( lsbuf, "%3X - %5X", i, regbuf[i] ) ;
    dbgprint ( lsbuf ) ;
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
bool putring ( uint8_t b )            // Put one byte in the ringbuffer
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
// Send a line of text to serial output.                                                   *
//******************************************************************************************
void dbgprint( const char* format, ... )
{
#if DEBUG_ESP_RADIO == 1
  va_list varArgs;
  va_start(varArgs, format);
  vsnprintf( sbuf, DEBUG_BUFFER_SIZE, format, varArgs) ;
  va_end(varArgs);
  Serial.print ( "D: " ) ;
  Serial.println ( sbuf ) ;
#endif
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
//******************************************************************************************
void listNetworks()
{
  int         maxsig = -1000 ;   // Used for searching strongest WiFi signal
  int         newstrength ;
  byte        encryption ;       // TKIP(WPA)=2, WEP=5, CCMP(WPA)=4, NONE=7, AUTO=8 
  const char* acceptable ;       // Netwerk is acceptable for connection
  int         i, j ;
  bool        found ;            // True if acceptable network found
  String      path ;             // Full filespec to see if SSID is an acceptable one
  
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
    path = String ( "/" ) + WiFi.SSID ( i ) + String ( ".pw" ) ;
    newstrength = WiFi.RSSI ( i ) ;
    if ( found = SPIFFS.exists ( path ) )                // Is this SSID acceptable?
    {
      acceptable = "Acceptable" ;
      if ( newstrength > maxsig )                        // This is a better Wifi
      {
        maxsig = newstrength ;
        ssid = WiFi.SSID ( i ) ;                         // Remember SSID name
      }
    }
    encryption = WiFi.encryptionType ( i ) ;
    dbgprint ( "%2d - %-25s Signal: %3d dBm Encryption %4s  %s",
                   i + 1, WiFi.SSID ( i ).c_str(), WiFi.RSSI ( i ),
                   getEncryptionType ( encryption ),
                   acceptable ) ;
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
  static uint32_t oldtotalcount = 7321 ;        // Needed foor change detection
  static uint8_t  morethanonce = 0 ;            // Counter for succesive fails
  static uint8_t  t600 = 0 ;                    // Counter for 10 minutes

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
      newpreset++ ;                             // Yes, try next channel
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
    if ( currentpreset != savpreset || mp3.getVolume() != savvolume )
    {
      // Volume or preset has changed, set request to save them in EEPROM 
      savreq = true ;
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
  int      oldmindist = 3000 ;                    // Detection least difference
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
//                                  T I M E R 1 0 0                                        *
//******************************************************************************************
// Examine button every 100 msec.                                                          *
//******************************************************************************************
void timer100()
{
  static int     count10sec = 0 ;                 // Counter for activatie 10 seconds process
  static int     oldval1 = HIGH ;                 // Previous value of digital input button 1
  static int     oldval2 = HIGH ;                 // Previous value of digital input button 2
  static int     oldval3 = HIGH ;                 // Previous value of digital input button 3
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
    #if ( not ( defined ( USETFT ) ) )
    newval = digitalRead ( BUTTON1 ) ;            // Test if below certain level
    if ( newval != oldval1 )                      // Change?
    {
      oldval1 = newval ;                          // Yes, remember value
      if ( newval == LOW )                        // Button pushed?
      {
        newpreset = 1 ;                           // Yes, goto station preset 1
        dbgprint ( "Digital button 1 pushed" ) ;
      }
      return ;
    }
    #endif
    newval = digitalRead ( BUTTON2 ) ;            // Test if below certain level
    if ( newval != oldval2 )                      // Change?
    {
      oldval2 = newval ;                          // Yes, remember value
      if ( newval == LOW )                        // Button pushed?
      {
        newpreset = currentpreset + 1 ;           // Yes, goto next preset station
        dbgprint ( "Digital button 2 pushed" ) ;
      }
      return ;
    }
    #if ( not ( defined ( USETFT ) ) )
    newval = digitalRead ( BUTTON3 ) ;            // Test if below certain level
    if ( newval != oldval3 )                      // Change?
    {
      oldval3 = newval ;                          // Yes, remember value
      if ( newval == LOW )                        // Button pushed?
      {
        newpreset = currentpreset - 1 ;           // Yes, goto previous preset station
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
        sprintf ( sbuf, "Analog button %d pushed, v = %d",
                  anewval, v ) ;
        dbgprint ( sbuf ) ;
        if ( anewval == 1 )                       // Button 1?
        {
          newpreset = 1 ;                         // Yes, goto preset 1
        }
        else if ( anewval == 2 )                  // Button 2?
        {
          newpreset = currentpreset + 1 ;         // Yes, goto next preset
        }
        else if ( anewval == 3 )                  // Button 3?
        {
          newpreset = currentpreset - 1 ;         // Yes, goto previous preset
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
    p1 = metaline + 12 ;                    // Begin of artist and title
    if ( p2 = strstr ( ml, ";" ) )          // Search for end of title
    {
      if ( *p1 == '\'' )                    // Surrounded by quotes?
      {
        p1++ ;
        p2-- ;
      }
      *p2 = '\0' ;                          // Strip the rest of the line
    }
    if ( *p1 == ' ' )                       // Leading space?
    {
      p1++ ;
    }
    // Save last part of string as streamtitle.  Protect against buffer overflow
    strncpy ( streamtitle, p1, sizeof ( streamtitle ) ) ;
    streamtitle[sizeof ( streamtitle ) - 1] = '\0' ;
  }
  else
  {
    return ;                                // Metadata does not contain streamtitle
  }
  if ( p1 = strstr ( streamtitle, " - " ) ) // look for artist/title separator
  {
    *p1++ = '\n' ;                          // Found: replace 3 characters by newline
    p2 = p1 + 2 ;
    if ( *p2 == ' ' )                       // Leading space in title?
    {
      p2++ ;
    }
    strcpy ( p1, p2 ) ;                     // Shift 2nd part of title 2 or 3 places
  }
  dbgprint ( ml ) ;
  displayinfo ( streamtitle, 20, CYAN ) ;   // Show title at position 20
}


//******************************************************************************************
//                            C O N N E C T T O H O S T                                    *
//******************************************************************************************
// Connect to the Internet radio server specified by newpreset.                            *
//******************************************************************************************
void connecttohost()
{
  int         i ;                                   // Index free EEPROM entry
  char*       eepromentry ;                         // Pointer to copy of EEPROM entry 
  char*       p ;                                   // Pointer in hostname
  const char* extension = "/" ;                     // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
  
  dbgprint ( "Connect to new host" ) ;
  if ( mp3client.connected() )
  {
    dbgprint ( "Stop client" ) ;                    // Stop conection to host
    mp3client.flush() ;
    mp3client.stop() ;
  }
  displayinfo ( "   ** Internet radio **", 0, WHITE ) ;
  if ( newstation[0] )                              // New station specified by host?
  {
    if ( strstr ( newstation, ":" ) )               // Correct format?
    {
      i = find_eeprom_station ( newstation ) ;      // See if already in EEPROM
      if ( i <= 0 )
      {                                             // Not yet in EEPROM
        i = find_free_eeprom_entry() ;              // Find free EEPROM entry (or entry 1)
        put_eeprom_station ( i, newstation ) ;      // Store new station
      }
      newpreset = i ;                               // Select this one
    }
    newstation[0] = '\0' ;                          // Handled this one 
  }
  if ( newpreset < 1 || newpreset > EENUM )         // Requested preset within limits?
  {
    newpreset = 1 ;                                 // No, reset to the first preset
  }
  while ( true )                                    // Find entry in hostlist that contains a colon.
  {                                                 // Will loop endlessly if empty list
    eepromentry = get_eeprom_station ( newpreset ) ;
    strcpy ( currentstat, eepromentry ) ;           // Save current station:port
    if ( strstr ( eepromentry, ":" ) )              // Check format
    {
      break ;                                       // Okay, leave loop
    }
    if ( ++newpreset == EENUM )
    {
       newpreset = 1 ;                              // Wrap around if beyond highest preset
    }
  }
  currentpreset = newpreset ;                       // This is the new preset
  strcpy ( host, eepromentry ) ;                    // Select first station number
  dbgprint ( "EEprom entry is %s", host ) ;    // The selected entry
  p = strstr ( host, ":" ) ;                        // Search for separator
  *p++ = '\0' ;                                     // Remove port from string and point to port
  port = atoi ( p ) ;                               // Get portnumber as integer
  // After the portnumber there may be an extension
  p = strstr ( p, "/" ) ;                           // Search for begin of extension
  if ( p )                                          // Is there an extension?
  {
    extension = p ;                                 // Yes, change the default
    dbgprint ( "Slash in station" ) ;
  }
  dbgprint ( "Connect to preset %d, host %s on port %d, extension %s",
            currentpreset, host, port, extension ) ;
  displayinfo ( sbuf, 60, YELLOW ) ;                // Show info at position 60
  delay ( 2000 ) ;                                  // Show for some time
  mp3client.flush() ;
  if ( mp3client.connect ( host, port ) )
  {
    dbgprint ( "Connected to server" ) ;
    // This will send the request to the server. Request metadata.
    mp3client.print ( String ( "GET " ) +
                      String ( extension ) +  
                      " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Icy-MetaData:1\r\n" +
                      "Connection: close\r\n\r\n");
  }
  datamode = INIT ;                                 // Start in metamode
}


//******************************************************************************************
//                     F I N D _ F R E E _ E E P R O M _ E N T R Y                         *
//******************************************************************************************
// Find a free EEPROM entry.  If none is found: return entry 1.                            *
//******************************************************************************************
int find_free_eeprom_entry()
{
  int   i ;                                      // Entry number
  char* p ;                                      // Pointer to entry

  for ( i = 1 ; i < EENUM ; i++ )
  {
    p = get_eeprom_station ( i ) ;               // Get next entry
    if ( *p == '\0' )                            // Is this one empty?
    {
      return i ;                                 // Yes, give index to caller
    }
  }
  return 1 ;                                     // No free entry, use the first
}


//******************************************************************************************
//                          G E T _ E E P R O M _ S T A T I O N                            *
//******************************************************************************************
// Get a station from EEPROM.  parameter index is 0..63.                                   *
// A pointer to the station will be returned.                                              *
//******************************************************************************************
char* get_eeprom_station ( int index )
{
  static char entry[EESIZ] ;            // One station from EEPROM
  int         i ;                       // index in entry
  int         address ;                 // Address in EEPROM 

  address = index * EESIZ ;             // Compute address in EEPROM
  for ( i = 0 ; i < EESIZ ; i++ )
  {
    entry[i] = EEPROM.read ( address++ ) ;
  }
  return entry ;                       // Geef pointer terug
}


//******************************************************************************************
//                          P U T _ E E P R O M _ S T A T I O N                            *
//******************************************************************************************
// Put a station into EEPROM.  1st parameter index is 0..63.                               *
// Note that 64 characters are copied.  The string "entry" may be shorter in reality, but  *
// that is generally no problem......                                                      *
//******************************************************************************************
void put_eeprom_station ( int index, const char *entry )
{
  int         i ;                            // index in entry
  int         address ;                      // Address in EEPROM

  address = index * EESIZ ;                  // Compute address in EEPROM
  for ( i = 0 ; i < EESIZ ; i++ )
  {
    EEPROM.write ( address++, entry[i] ) ; // Copy 1 character
  }
  yield() ;
  EEPROM.commit() ;                          // Commit the write
}


//******************************************************************************************
//                          F I N D _ E E P R O M _ S T A T I O N                          *
//******************************************************************************************
// Search for a station in EEPROM.  Return the index or 0 if not found.                    *
//******************************************************************************************
int find_eeprom_station ( const char *search_entry )
{
  char*       p ;                                 // Pointer to entry
  int         entnum ;                            // Entry number
  int         i ;                                 // index in entry
  int         address ;                           // Address in EEPROM 

  for ( entnum = 1 ; entnum < EENUM ; entnum++ )
  {
    p = get_eeprom_station ( entnum ) ;           // Get next entry
    if ( strstr ( p, search_entry ) )             // Matches entry?
    {
      return entnum ;                             // Yes, return index
    }
  }
  return 0 ;                                      // No match found
}


//******************************************************************************************
//                  P U T _ E M P T Y _ E E P R O M _ S T A T I O N                        *
//******************************************************************************************
// Put an empty station into EEPROM.  parameter index is 1..63.                            *
//******************************************************************************************
void put_empty_eeprom_station ( int index )
{
  int         i ;                      // index in entry
  int         address ;                // Address in EEPROM 

  address = index * EESIZ ;            // Compute address in EEPROM
  for ( i = 0 ; i < EESIZ ; i++ )
  {
    EEPROM.write ( address++, 0 ) ;
  }
  yield() ;
  EEPROM.commit() ;                    // Commit the write
}


//******************************************************************************************
//                               F I L L _ E E P R O M                                     *
//******************************************************************************************
// Setup EEPROM if empty.                                                                  *
//******************************************************************************************
void fill_eeprom()
{
  int  i ;                                           // Entry number
  int  j ;                                           // Index in hostlist
  char *p ;                                          // Pointer to copy of EEPROM entry
  int  fillflag ;                                    // Count of non-empty lines in EEPROM
  
  // See if first entry in EEPROM makes sense
  p = get_eeprom_station ( 0 ) ;                     // Get reserved entry to check status
  if ( strcmp ( p, hostlist[0] ) == 0 )              // Starts with a familiar pattern?
  {
    // Yes, show the list for debugging purposes
    dbgprint ( "EEPROM is already filled. Available stations:" ) ;
    for ( i = 0 ; i < EENUM ; i++ )                  // List all for entries
    {
      p = get_eeprom_station ( i ) ;                 // Get next entry
      if ( *p )                                      // Check if filled with a station
      { 
        fillflag = i ;                               // > 0 if at least one line is filled
        dbgprint ( "%02d - %s",
                  i, get_eeprom_station ( i ) ) ;
      }
    }
    if ( fillflag )
    {
      return ;                                       // EEPROM already filled
    }
  }    
  // EEPROM is virgin or empty.  Fill it with default stations.
  dbgprint ( "EEPROM is empty.  Will be filled now." ) ;
  delay ( 300 ) ;
  j = 0 ;                                            // Point to first line in hostlist
  for ( i = 0 ; i < EENUM ; i++ )                    // Space for all entries
  {
    if ( hostlist[j] )                               // At last host in the list?
    { 
      put_eeprom_station ( i, hostlist[j++] ) ;      // Copy station
    }
    else
    {
      put_empty_eeprom_station ( i ) ;              // Fill with a zero pattern
    }
    yield() ;
  }
}


//******************************************************************************************
//                               C O N N E C T W I F I                                     *
//******************************************************************************************
// Connect to WiFi using passwords available in the SPIFFS.                                *
//******************************************************************************************
void connectwifi()
{
  String path ;                                        // Full file spec
  String pw ;                                          // Password from file
  File   pwfile ;                                      // File containing password for WiFi
  
  path = String ( "/" )  + ssid + String ( ".pw" ) ;   // Form full path
  pwfile = SPIFFS.open ( path, "r" ) ;                 // File name equal to SSID
  pw = pwfile.readStringUntil ( '\n' ) ;               // Read password as a string
  pw.trim() ;                                          // Remove CR                              
  WiFi.begin ( ssid.c_str(), pw.c_str() ) ;            // Connect to selected SSID
  sprintf ( sbuf, "Try WiFi %s",
            ssid.c_str() ) ;                           // Message to show during WiFi connect
  if (  WiFi.waitForConnectResult() != WL_CONNECTED )  // Try to connect
  {
    dbgprint ( "WiFi Failed!" ) ;
    return;
  }
  sprintf ( sbuf, "IP = %d.%d.%d.%d",
                  WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] ) ;
  dbgprint ( sbuf ) ;
  #if defined ( USETFT )
  tft.println ( sbuf ) ;
  #endif
}


//******************************************************************************************
//                      S A V E V O L U M E A N D P R E S E T                              *
//******************************************************************************************
// Save current volume and preset in the first entry of the EEPROM.                        *
// This info is kept in the last 2 bytes of the entry.                                     *                                                                  *
//******************************************************************************************
void saveVolumeAndPreset()
{
  char*  p ;                                         // Points into entry 0

  savvolume = mp3.getVolume() ;                      // Current volume
  savpreset = currentpreset ;                        // Current preset station
  p = get_eeprom_station ( 0 ) ;                     // Point to entry 0
  p[EESIZ-2] = savvolume ;                           // Save volume
  p[EESIZ-1] = currentpreset ;                       // Save preset station
  put_eeprom_station ( 0, p ) ;                      // Put in EEPROM
  dbgprint ( "Settings saved: volume %d, preset %d",
            savvolume, savpreset ) ;
}


//******************************************************************************************
//                   R E S T O R E V O L U M E A N D P R E S E T                           *
//******************************************************************************************
// See if there is a saved volume and preset in the first entry of the EEPROM.             *
// This info is kept in the last 2 bytes of the entry.                                     *                                                                  *
//******************************************************************************************
void restoreVolumeAndPreset()
{
  char*  p ;                                         // Points into entry 0

  p = get_eeprom_station ( 0 ) ;                     // Point to entry 0
  p = p + EESIZ - 2 ;                                // Point to volume byte
  if ( *p > 60 && *p <= 100  )
  {
    reqvol = *p++ ;                                  // Restore saved volume
    newpreset = *p ;                                 // Restore saved preset
    savvolume = reqvol ;                             // Saved volume
    savpreset = newpreset ;                          // Saved preset station
    dbgprint ( "Restored settings: volume %d, preset %d",
              reqvol, newpreset ) ;
  }
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
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup()
{
  FSInfo      fs_info ;                              // Info about SPIFFS
  Dir         dir ;                                  // Directory struct for SPIFFS
  File        f ;                                    // Filehandle
  String      filename ;                             // Name of file found in SPIFFS
  String      potSSID ;                              // Potential SSID if only 1 one password file
  int         i ;                                    // Loop control
  int         numpwf = 0 ;                           // Number of password files 

  Serial.begin ( 115200 ) ;                          // For debug
  Serial.println() ;
  system_update_cpu_freq ( 160 ) ;                   // Set to 80/160 MHz
  ringbuf = (uint8_t *) malloc ( RINGBFSIZ ) ;       // Create ring buffer
  SPIFFS.begin() ;                                   // Enable file system
  // Show some info about the SPIFFS
  SPIFFS.info ( fs_info ) ;
  dbgprint ( "FS Total %d, used %d", fs_info.totalBytes, fs_info.usedBytes ) ;
  dir = SPIFFS.openDir("/") ;                        // Show files in FS
  while ( dir.next() )                               // All files
  {
    f = dir.openFile ( "r" ) ;
    filename = dir.fileName() ;
    dbgprint ( "%-32s - %6d",                   // Show name and size
              filename.c_str(), f.size() ) ;
    if ( filename.endsWith ( ".pw" ) )               // If this a password file?
    {
      numpwf++ ;                                     // Yes, count number password files
      potSSID = filename.substring ( 1 ) ;           // Save filename (without starting "/") of potential SSID 
      potSSID.replace ( ".pw", "" ) ;                // Convert into SSID 
    }
  }
  WiFi.mode ( WIFI_STA ) ;                           // This ESP is a station
  wifi_station_set_hostname ( (char*)"ESP-radio" ) ; 
  SPI.begin() ;                                      // Init SPI bus
  EEPROM.begin ( EEPROM_SIZE ) ;                     // For station list in EEPROM
  dbgprint (                                    // Some memory info
            "Starting ESP Version 17-05-2016...  Free memory %d",
            system_get_free_heap_size() ) ;
  dbgprint (                                    // Some sketch info
            "Sketch size %d, free size %d",
            ESP.getSketchSize(),
            ESP.getFreeSketchSpace() ) ;
  fill_eeprom() ;                                    // Fill if empty
  restoreVolumeAndPreset() ;                         // Restore saved settings
  pinMode ( BUTTON2, INPUT_PULLUP ) ;                // Input for control button 2
  mp3.begin() ;                                      // Initialize VS1053 player
  # if defined ( USETFT )
  tft.begin() ;                                      // Init TFT interface
  tft.fillRect ( 0, 0, 160, 128, BLACK ) ;           // Clear screen does not work when rotated
  tft.setRotation ( 3 ) ;                            // Use landscape format
  tft.clearScreen() ;                                // Clear screen
  tft.setTextSize ( 1 ) ;                            // Small character font
  tft.setTextColor ( WHITE ) ;  
  tft.println ( "Starting" ) ;
  #else
  pinMode ( BUTTON1, INPUT_PULLUP ) ;                // Input for control button 1
  pinMode ( BUTTON3, INPUT_PULLUP ) ;                // Input for control button 3
  #endif
  delay(10);
  streamtitle[0] = '\0' ;                            // No title yet
  newstation[0] = '\0' ;                             // No new station yet
  tckr.attach ( 0.100, timer100 ) ;                  // Every 100 msec
  listNetworks() ;                                   // Search for strongest WiFi network
  if ( numpwf == 1 )                                 // If there's only one pw-file...
  {
    dbgprint ( "Single (hidden) SSID found" ) ;
    ssid = potSSID ;                                 // Use this SSID (it may be hidden)
  }
  dbgprint ( "Selected network: %-25s", ssid.c_str() ) ;
  connectwifi() ;                                    // Connect to WiFi network
  dbgprint ( "Start server for commands" ) ;
  cmdserver.on ( "/", handleCmd ) ;                  // Handle startpage
  cmdserver.onNotFound ( handleFS ) ;                // Handle file from FS
  cmdserver.begin() ;
  delay ( 1000 ) ;                                   // Show IP for a wile
  connecttohost() ;                                  // Connect to the selected host
  ArduinoOTA.setHostname ( "ESP-radio" ) ;           // Set the hostname
  ArduinoOTA.onStart ( otastart ) ;
  ArduinoOTA.begin() ;                               // Allow update over the air
  analogrest = ( analogRead ( A0 ) + asw1 ) / 2  ;   // Assumed inactive analog input
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
  // Try to keep the ringbuffer filled up by adding as much bytes as possible 
  while ( ringspace() && mp3client.available() )
  {
    putring ( mp3client.read() ) ;                // Yes, save one byte in ringbuffer
  }
  yield() ;
  while ( mp3.data_request() && ringavail() )     // Try to keep VS1053 filled
  {
    handlebyte ( getring() ) ;                    // Yes, handle it
  }
  if ( delpreset )                                // Delete preset requested?
  {
    put_empty_eeprom_station ( delpreset ) ;      // Fill with a zero pattern
    if ( delpreset == currentpreset )             // Listening to a deleted station?
    {
      newpreset++ ;                               // Yes, select the next preset
    }
    delpreset = 0 ;                               // Just once
  }
  if ( newpreset != currentpreset )               // New station requested?
  {
    mp3.setVolume ( 0 ) ;                         // Mute
    mp3.stopSong() ;                              // Stop playing
    emptyring() ;                                 // Empty the ringbuffer
    connecttohost() ;                             // Switch to new host
  }
  if ( savreq )                                   // Request to save settings
  {
    savreq = false ;                              // Yes: reset request first
    saveVolumeAndPreset() ;                       // Save the settings
  }
  mp3.setVolume ( reqvol ) ;                      // Set to requested volume
  if ( reqtone )                                  // Request to change tone?
  {
    reqtone = false ;
    mp3.setTone ( rtone ) ;                       // Set SCI_BASS to requested value
  }
  ArduinoOTA.handle() ;                           // Check for OTA
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
  static int       metaindex ;                          // Index in metaline
  static bool      firstmetabyte ;                      // True if first metabyte (counter) 
  static int       LFcount ;                            // Detection of end of header
  static __attribute__((aligned(4))) uint8_t buf[32] ;  // Buffer for chunk
  static int       chunkcount = 0 ;                     // Data in chunk
  static bool      firstchunk = true ;                  // First chunk as input
  char*            p ;                                  // Pointer in metaline
  int              i, j ;                               // Loop control

  
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
      if ( p = strstr ( metaline, "icy-br:" ) )
      {
        bitrate = atoi ( p + 7 ) ;                     // Found bitrate tag, read the bitrate
        if ( bitrate == 0 )                            // For Ogg br is like "Quality 2"
        {
          bitrate = 87 ;                               // Dummy bitrate
        }
      }
      else if ( p = strstr ( metaline, "icy-metaint:" ) )
      {
        metaint = atoi ( p + 12 ) ;                    // Found metaint tag, read the value
      }
      else if ( p = strstr ( metaline, "icy-name:" ) )
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
        sprintf ( sbuf, "Metadata block %d bytes",
                  metacount-1 ) ;                      // Most of the time there are zero bytes of metadata
        dbgprint ( sbuf ) ;
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
  int                 i ;                          // Loop control
  char                *p ;                         // Pointer to EEprom entry
  AsyncResponseStream *response ;                  // Response to client
  
  response = request->beginResponseStream ( "text/plain" ) ;
  for ( i = 1 ; i < EENUM ; i++ )                  // List all for entries
  {
    p = get_eeprom_station ( i ) ;                 // Get next entry
    if ( *p )                                      // Check if filled with a station
    { 
      response->printf ( "%02d%s|", i, p ) ;      // 2 digits plus name   
    }
  }
  request->send ( response ) ;
}


//******************************************************************************************
//                             H A N D L E C M D                                           *
//******************************************************************************************
// Handling of the various commands from remote (case sensitive). All commands start with  *
// "/command", followed by "?parameter=value".  Example: "/command?volume=50".             *
// The startpage will be returned if no arguments are given.                               *
// Examples with available parameters:                                                     *
//   volume     = 95                        // Percentage between 0 and 100                *
//   upvolume   = 2                         // Add percentage to current volume            *
//   downvolume = 2                         // Subtract percentage from current volume     *
//   preset     = 5                         // Select preset 5 station for listening       *
//   uppreset   = 1                         // Select next preset station for listening    *
//   downpreset = 1                         // Select previous preset station              *
//   tone       = 3,4,3,4                   // Setting bass and treble (see documentation) *
//   station    = address:port              // Store new preset station and select it      *
//   delete     = 0                         // Delete current playing station              *
//   delete     = 5                         // Delete preset station number 5              *
//   status     = 0                         // Show current station:port                   *
//   test       = 0                         // For test purposes                           *
//   debug      = 0 or 1                    // Switch debugging on or off                  *
//   list       = 0                         // Returns list of presets                     *
// Multiple parameters are ignored.  An extra parameter may be "version=<random number>"   *
// in order to prevent browsers like Edge and IE to use their cache.  This "version" is    *
// ignored.                                                                                *
// Example: "/command?upvolume=5&version=0.9775479450590543"                               *
//******************************************************************************************
void handleCmd ( AsyncWebServerRequest *request )
{
  AsyncWebParameter* p ;                              // Points to parameter structure
  String             argument ;                       // Next argument in command
  String             value ;                          // Value of an argument
  int                ivalue ;                         // Value of argument as an integer
  static char        reply[80] ;                      // Reply to client
  uint8_t            oldvol ;                         // Current volume
  uint8_t            newvol ;                         // Requested volume
  int                numargs ;                        // Number of arguments
  int                i ;                              // Loop through the commands
  bool               relative ;                       // Relative argument (+ or -)
  uint32_t           t ;                              // For time test
  
  t = millis() ;                                      // Timestamp at start
  numargs = request->params() ;                       // Get number of arguments
  if ( numargs == 0 )                                 // Any arguments
  {
    request->send ( SPIFFS, "/index.html",            // No parameters, send the startpage
                    "text/html" ) ;
    return ;
  }
  strcpy ( reply, "Command(s) accepted" ) ;           // Default reply
  p = request->getParam ( i ) ;                       // Get pointer to parameter structure
  argument = p->name() ;                              // Get the argument
  argument.toLowerCase() ;                            // Force to lower case
  value = p->value() ;                                // Get the specified value
  ivalue = abs ( value.toInt() ) ;                    // Also as an integer
  relative = argument.indexOf ( "up" ) == 0 ;         // + relative setting?
  if ( argument.indexOf ( "down" ) == 0 )             // - relative setting?
  {
    relative = true ;                                 // It's relative
    ivalue = - ivalue ;                               // But with negative value
  }
  dbgprint ( "Command: %s with parameter %s converted to %d",
            argument.c_str(), value.c_str(), ivalue ) ;
  if ( argument.indexOf ( "volume" ) >= 0 )           // Volume setting?
  {
    // Volume may be of the form "volume+", "volume-" or "volume" for relative or absolute setting
    oldvol = mp3.getVolume() ;                        // Get current volume
    if ( relative )                                   // + relative setting?
    {
      reqvol = oldvol + ivalue ;                      // Up by 0.5 or more dB
    }
    else
    {
      reqvol = ivalue ;                               // Absolue setting
    }
    if ( reqvol > 100 )
    {
      reqvol = 100 ;                                  // Limit to normal values
    }
    sprintf ( reply, "Volume is now %d", reqvol ) ;   // Reply new volume
  }
  else if ( argument.indexOf ( "preset" ) >= 0 )      // Preset station?
  {
    if ( relative )                                   // Relative argument?
    {
      newpreset += ivalue ;                           // Yes, adjust currentpreset
    }
    else
    {
      newpreset = ivalue ;                            // Otherwise set station
    }
    sprintf ( reply, "Next station requested = %d",   // Format reply
              newpreset ) ;
  }
  else if ( argument.indexOf ( "station" ) == 0 )     // Station in the form address:port
  {
    strcpy ( newstation, value.c_str() ) ;            // Save it for storage and selection later 
    newpreset++ ;                                     // Select this station as new preset
    sprintf ( reply,
              "New preset station %s accepted",       // Format reply
              newstation ) ;
  }
  else if ( argument.indexOf ( "delete" ) == 0 )      // Station in the form address:port
  {
    if ( ivalue < 0 || ivalue >= EENUM )              // Check preset number
    {
      sprintf ( reply, "Bad preset number %d",        // Must be 0..63
                ivalue ) ;
    }
    if ( ivalue )                                     // 0 means current preset
    {
      delpreset = ivalue ;                            // Fill with a zero pattern later
    }
    else
    {
      delpreset = currentpreset ;                     // Fill with a zero pattern later
    }
  }
  else if ( argument.indexOf ( "status" ) == 0 )      // Status request
  {
    sprintf ( reply, "Playing preset %d - %s",        // Format reply
              currentpreset, currentstat ) ;
  }
  else if ( argument.indexOf ( "reset" ) == 0 )       // Reset request
  {
    ESP.restart() ;                                   // Reset all
    // No continuation here......
  }
  else if ( argument.indexOf ( "test" ) == 0 )        // Test command
  {
    sprintf ( reply, "Free memory is %d, ringbuf %d, stream %d",
              system_get_free_heap_size(), rcount, mp3client.available() ) ;
  }
  // Commands for bass/treble control
  else if ( argument.indexOf ( "tone" ) == 0 )        // Tone command
  {
    if ( argument.indexOf ( "ha" ) > 0 )              // High amplitue? (for treble)
    {
      rtone[0] = ivalue ;                             // Yes, prepare to set ST_AMPLITUDE
    }
    if ( argument.indexOf ( "hf" ) > 0 )              // High frequency? (for treble)
    {
      rtone[1] = ivalue ;                             // Yes, prepare to set ST_FREQLIMIT
    }
    if ( argument.indexOf ( "la" ) > 0 )              // Low amplitue? (for bass)
    {
      rtone[2] = ivalue ;                             // Yes, prepare to set SB_AMPLITUDE
    }
    if ( argument.indexOf ( "lf" ) > 0 )              // High frequency? (for bass)
    {
      rtone[3] = ivalue ;                             // Yes, prepare to set SB_FREQLIMIT
    }
    reqtone = true ;                                  // Set change request
    sprintf ( reply, "Parameter for bass/treble %s set to %d",
              argument.c_str(), ivalue ) ;
  }
  else if ( argument.indexOf ( "list" ) == 0 )        // list request
  {
    getpresets ( request ) ;                          // Yes, get the list and send it as reply
    return ; 
  }
  else if ( argument.indexOf ( "analog" ) == 0 )      // list request
  {
    sprintf ( reply, "Analog input = %d units",       // Read the analog input for test
              analogRead ( A0 ) ) ;
  }
  else
  {
    sprintf ( reply, "ESP-radio called with ilegal parameter: %s",
              argument.c_str() ) ;
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

