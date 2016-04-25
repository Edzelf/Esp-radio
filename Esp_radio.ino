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
//  icy-br:128                 - in kb/sec 
//
// After de double CRLF is received, the server starts sending mp3-data.  This data contains
// metadata (non mp3) after every "metaint" mp3 bytes.  This metadata is empty in most cases,
// but if any is available the content will be presented on the TFT.
// Pushing the input button causes the player to select the next station in the hostlist (EEPROM).
//
// The display used is a Chinese 1.8 color TFT module 128 x 160 pixels.  The TFT_ILI9163C.h
// file has been changed to reflect this particular module.  TFT_ILI9163C.cpp has been
// changed to use the full screenwidth if rotated to mode "3".  Now there is room for 26
// characters per line and 16 lines.  Software will work without installing the display.
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
// D3       GPIO0    0 FLASH        -                   -                    Control button
// D4       GPIO2    2              pin 3 (D/C)         -                    -
// D5       GPIO14  14 SCLK         pin 5 (CLK)         pin 5 SCK            -
// D6       GPIO12  12 MISO         -                   pin 7 MISO           -
// D7       GPIO13  13 MOSI         pin 4 (DIN)         pin 6 MOSI           -
// D8       GPIO15  15              pin 2 (CS)          -                    -
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
// The program will reset the ESP8266 in such a case.  It seems that this error will show up
// less frequently if DNS is not used.  Now we have switched to async webserver, maybe that
// results in a better stability.
//
// 31-03-2016, ES: First set-up.
// 01-04-2016, ES: Detect missing VS1053 at start-up.
// 05-04-2016, ES: Added commands through http server on port 80.
// 06-04-2016, ES: Added list of stations in EEPROM
// 14-04-2016, ES: Added icon and switch preset on stream error.
// 18-04-2016, ES: Added SPIFFS for webserver
// 19-04-2016, ES: Added ringbuffer
// 20-04-2016, ES: WiFi Passwords through SPIFFS files, enable OTA
// 21-04-2016, ES: Switch to Async Webserver
//
// Define dns if you want a DNS reponder.  Mind the "LmacRxBlk:1" errors....
#define  dns 1
#if defined ( dns )
  #include <ESP8266mDNS.h>
#endif
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
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

// Color definitions for the TFT screen
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
// Pins CS and DC for TFT module
#define TFT_CS 15
#define TFT_DC 2
// Control button for controlling station
#define BUTTON 0
// Maximal number of presets in EEPROM and size of an entry
#define EENUM 64
#define EESIZ 64
// Ringbuffer for smooth playing 20000 bytes is 160 Kbits, about 1.5 seconds at 128kb bitrate.
#define RINGBFSIZ 20000

//******************************************************************************************
// Global data section.                                                                    *
//******************************************************************************************
enum datamode_t { INIT, HEADER, DATA, METADATA } ;         // State for datastream
// Global variables
String           ssid ;                                    // SSID of selected WiFi network
int              DEBUG = 1 ;
#if defined ( dns )
  MDNSResponder  mdns ;                                    // The MDNS responder
#endif
WiFiClient       mp3client ;                               // An instance of the mp3 client
AsyncWebServer   cmdserver ( 80 ) ;                        // Instance of embedded webserver
String           cmd ;                                     // Command from remote
TFT_ILI9163C     tft = TFT_ILI9163C ( TFT_CS, TFT_DC ) ;
Ticker           tckr ;                                    // For timing 10 sec
uint32_t         totalcount = 0 ;                          // Counter mp3 data
char             sbuf[100] ;                               // For debug lines
datamode_t       datamode ;                                // State of datastream
int              metacount ;                               // Number of bytes in metadata
int              datacount ;                               // Counter databytes before metadata
char             metaline[150] ;                           // Readable line in metadata
char             streamtitle[150] ;                        // Streamtitle from metadata
int              bitrate = 0 ;                             // Bitrate in kb/sec            
int              metaint = 0 ;                             // Number of databytes between metadata
int              currentpreset = 1 ;                       // Preset station to play (index in hostlist (EEPROM))
int              newpreset = 1 ;                           // Requested preset
char             host[EESIZ] ;                             // The hostname
char             sname[100] ;                              // Stationname
int              port ;                                    // Port number for host
char             newstation[EESIZ] ;                       // Station:port from remote
int              delpreset = 0 ;                           // Preset to be deleted if nonzero
uint8_t          reqvol = 80 ;                             // Requested volume
char             currentstat[EESIZ] ;                      // Current station:port
uint8_t*         ringbuf ;                                 // Ringbuffer for VS1053
uint16_t         rbwindex = 0 ;                            // Fill pointer in ringbuffer
uint16_t         rbrindex = RINGBFSIZ - 1 ;                // Emptypointer in ringbuffer
uint16_t         rcount = 0 ;                              // Number of bytes in ringbuffer
//
// List of initial preset stations.
// This will be copied to EEPROM if EEPROM is empty.  The first entry [0] is reserved
// for detection of a not yet filled EEPROM.
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
                     "109.206.96.11:80",              //  9 - TOP FM Beograd 106,8  64-kpbs
                     "85.17.121.216:8468",            // 10 - RADIO LEHOVO 971 GREECE, 64-kbps
                     "85.17.121.103:8800",            // 11 - STAR FM 88.8 Corfu Greece, 64-kbps
                     "85.17.122.39:8530",             // 12 - stylfm.gr laiko, 64-kbps
                     "94.23.66.155:8106",             // 13 - *ILR CHILL & GROOVE* 64-kbps
                     "205.164.62.22:7012",            // 14 - 1.FM - ABSOLUTE TRANCE (EURO) RADIO 64-kbps
                     NULL } ;

//******************************************************************************************
// End of lobal data section.                                                              *
//******************************************************************************************

void dbgprint ( const char* p ) ;   // Forward declaration for VS1053 stuff

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
        yield() ;                         // Very short delay
    }
  }
  
  inline void control_mode_on() const
  {
    SPI.beginTransaction ( VS1053_SPI ) ; // Prevent other SPI users
    digitalWrite ( dcs_pin, HIGH ) ;      // Bring slave in dontrol mode
    digitalWrite ( cs_pin, LOW ) ;
  }

  inline void control_mode_off() const
  {
    digitalWrite ( cs_pin, HIGH ) ;       // End control mode
    SPI.endTransaction() ;                // Allow other SPI users
  }

  inline void data_mode_on() const
  {
    SPI.beginTransaction ( VS1053_SPI ) ; // Prevent other SPI users
    digitalWrite ( cs_pin, HIGH ) ;       // Bring slave in data mode
    digitalWrite ( dcs_pin, LOW ) ;
  }

  inline void data_mode_off() const
  {
    digitalWrite ( dcs_pin, HIGH ) ;      // End data mode
    SPI.endTransaction() ;                // Allow other SPI users
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

// Constructor
VS1053::VS1053 ( uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin ) :
  cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin)
{
}

uint16_t VS1053::read_register ( uint8_t _reg ) const
{
  uint16_t result ;
  
  control_mode_on() ;
  SPI.write ( 3 ) ;                          // Read operation
  SPI.write ( _reg ) ;                       // Register to write (0..0xF)
  // Note: transfer16 does not seem to work
  result = ( SPI.transfer ( 0xFF ) << 8 ) |  // Read 16 bits data
           ( SPI.transfer ( 0xFF ) ) ;
  await_data_request() ;                     // Wait for DREQ to be HIGH again
  control_mode_off() ;
  return result ;
}

void VS1053::write_register ( uint8_t _reg, uint16_t _value ) const
{
  control_mode_on( );
  SPI.write ( 2 ) ;                          // Write operation
  SPI.write ( _reg ) ;                       // Register to write (0..0xF)
  SPI.write16 ( _value ) ;                   // Send 16 bits data  
  await_data_request() ;
  control_mode_off() ;
}

void VS1053::sdi_send_buffer ( uint8_t* data, size_t len )
{
  size_t chunk_length ;                      // Length of chunk 32 byte or shorter
  
  data_mode_on() ;
  while ( len )                              // More to do?
  {
    await_data_request() ;                   // Wait for space available
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
  size_t chunk_length ;                      // Length of chunk 32 byte or shorter
  
  data_mode_on() ;
  while ( len )                              // More to do?
  {
    await_data_request() ;                   // Wait for space available
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
  wram_write ( 0xC017, 3 ) ;                            // Switch to MP3 mode
  wram_write ( 0xC019, 0 ) ;                            // Switch to MP3 mode
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
void dbgprint ( const char* p )
{
  if ( DEBUG )
  {
    Serial.print ( "D: " ) ;
    Serial.println ( p ) ;
  }
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
  sprintf ( sbuf, "Number of available networks: %d",
            numSsid ) ;
  dbgprint ( sbuf ) ;

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
    sprintf ( sbuf, "%2d - %-25s Signal: %3d dBm Encryption %4s  %s",
                   i + 1, WiFi.SSID ( i ).c_str(), WiFi.RSSI ( i ),
                   getEncryptionType ( encryption ),
                   acceptable ) ;
    dbgprint ( sbuf ) ;
  }
  dbgprint ( "--------------------------------------" ) ;
  sprintf ( sbuf, "Selected network: %-25s", ssid.c_str() ) ;
  dbgprint ( sbuf ) ;
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
//                                  T I M E R 1 0 S E C                                    *
//******************************************************************************************
// Extra watchdog.  Called every 10 seconds.                                               *
// If totalcount has not been changed, there is a problem and a reset will be performed.   *
//******************************************************************************************
void timer10sec()
{
  static uint32_t oldtotalcount = 7321 ;        // Needed foor change detection
  static uint8_t  morethanonce = 0 ;            // Counter for succesive fails
  
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
}


//******************************************************************************************
//                                  T I M E R 1 0 0                                        *
//******************************************************************************************
// Examine button every 100 msec.                                                          *
//******************************************************************************************
void timer100()
{
  static int  count10sec = 0 ;
  static int  oldval = HIGH ;
  int         newval ;
  
  if ( ++count10sec == 100  )                 // 10 seconds passed?
  {
    timer10sec() ;                            // Yes, do 10 second procedure
    count10sec = 0 ;                          // Reset count
  }
  else
  {
    newval = digitalRead ( BUTTON ) ;         // Test if below certain level
    if ( newval != oldval )
    {
      oldval = newval ;                       // Remember value
      if ( newval == LOW )                    // Button pushed?
      {
        newpreset = currentpreset + 1 ;       // Remember action
        //dbgprint ( "Button pushed" ) ;
      }
    }
  }
  while ( mp3.data_request() && ringavail() )     // Try to keep VS1053 filled
  {
    handlebyte ( getring() ) ;                    // Yes, handle it
  }
}


//******************************************************************************************
//                            C O N N E C T T O H O S T                                    *
//******************************************************************************************
// Connect to the Internet radio server specified by newpreset.                            *
//******************************************************************************************
void connecttohost()
{
  int      i ;                                      // Index free EEPROM entry
  char*    eepromentry ;                            // Pointer to copy of EEPROM entry 
  char*    p ;                                      // Pointer in hostname

  
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
  p = strstr ( host, ":" ) ;                        // Search for separator
  *p++ = '\0' ;                                     // Remove port from string and point to port
  port = atoi ( p ) ;                               // Get portnumber as integer
  sprintf ( sbuf, "Connect to preset %d, host %s on port %d",
            currentpreset, host, port ) ;
  dbgprint ( sbuf ) ;
  displayinfo ( sbuf, 60, YELLOW ) ;                // Show info at position 60
  delay ( 2000 ) ;                                  // Show for some time
  mp3client.flush() ;
  if ( mp3client.connect ( host, port ) )
  {
    dbgprint ( "Connected to server" ) ;
    // This will send the request to the server. Request metadata.
    mp3client.print ( String ( "GET / HTTP/1.1\r\n" ) +
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
    yield() ;
  }
  return entry ;                       // Geef pointer terug
}


//******************************************************************************************
//                          P U T _ E E P R O M _ S T A T I O N                            *
//******************************************************************************************
// Put a station into EEPROM.  1st parameter index is 0..63.                               *
//******************************************************************************************
void put_eeprom_station ( int index, const char *entry )
{
  int         i ;                      // index in entry
  int         address ;                // Address in EEPROM 

  address = index * EESIZ ;            // Compute address in EEPROM
  for ( i = 0 ; i < EESIZ ; i++ )
  {
    EEPROM.write ( address++, entry[i] ) ;
  }
  yield() ;
  EEPROM.commit() ;                    // Commit the write
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
  for ( i = 0 ; i < EENUM ; i++ )
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
        sprintf ( sbuf, "%02d - %s",
                  i, get_eeprom_station ( i ) ) ;
        dbgprint ( sbuf ) ;
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
  for ( i = 0 ; i < EESIZ ; i++ )                    // Space for all entries
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
  while ( WiFi.status() != WL_CONNECTED )
  {
    dbgprint ( sbuf ) ;                                // Show activity
    tft.println ( sbuf ) ;
    delay ( 2000 ) ;
  }
  sprintf ( sbuf, "IP = %d.%d.%d.%d",
                  WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] ) ;
  dbgprint ( sbuf ) ;
  tft.println ( sbuf ) ;
}


//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup()
{
  FSInfo      fs_info ;
  byte        mac[6] ;
  int         i ;

  Serial.begin ( 115200 ) ;                          // For debug
  Serial.println() ;
  system_update_cpu_freq ( 160 ) ;                   // Set to 80/160 MHz
  ringbuf = (uint8_t *) malloc ( RINGBFSIZ ) ;       // Create ring buffer
  SPIFFS.begin() ;                                   // Enable file system
  // Show some info about the SPIFFS
  SPIFFS.info ( fs_info ) ;
  sprintf ( sbuf, "FS Total %d, used %d", fs_info.totalBytes, fs_info.usedBytes ) ;
  dbgprint ( sbuf ) ;
  Dir dir = SPIFFS.openDir("/") ;                    // Show files in FS
  while ( dir.next() )                               // All files
  {
    File f = dir.openFile ( "r" ) ;
    String filename = dir.fileName() ;
    sprintf ( sbuf, "%-32s - %6d",                   // Show name and size
              filename.c_str(), f.size() ) ;
    dbgprint ( sbuf ) ;
  }
  WiFi.mode ( WIFI_STA ) ;                           // This ESP is a station
  wifi_station_set_hostname ( (char*)"ESP-radio" ) ; 
  SPI.begin() ;                                      // Init SPI bus
  EEPROM.begin ( 2048 ) ;                            // For station list in EEPROM
  sprintf ( sbuf,                                    // Some memory info
            "Starting ESP V2.1...  Free memory %d",
            system_get_free_heap_size() ) ;
  dbgprint ( sbuf ) ;
  sprintf ( sbuf,                                    // Some sketch info
            "Sketch size %d, free size %d",
            ESP.getSketchSize(),
            ESP.getFreeSketchSpace() ) ;
  dbgprint ( sbuf ) ;
  fill_eeprom() ;                                    // Fill if empty  
  pinMode ( BUTTON, INPUT ) ;                        // Input for control button
  mp3.begin() ;                                      // Initialize VS1053 player
  tft.begin() ;                                      // Init TFT interface
  tft.fillRect ( 0, 0, 160, 128, BLACK ) ;           // Clear screen does not work when rotated
  tft.setRotation ( 3 ) ;                            // Use landscape format
  tft.clearScreen() ;                                // Clear screen
  tft.setTextSize ( 1 ) ;                            // Small character font
  tft.setTextColor ( WHITE ) ;  
  tft.println ( "Starting" ) ;
  delay(10);
  streamtitle[0] = '\0' ;                            // No title yet
  newstation[0] = '\0' ;                             // No new station yet
  tckr.attach ( 0.100, timer100 ) ;                  // Every 100 msec
  listNetworks() ;                                   // Search for strongest WiFi network
  connectwifi() ;                                    // Connect to WiFi network
  #if defined ( dns )
    dbgprint ( "Start MDNS responder" ) ;
    if ( !mdns.begin ( "ESP-radio",
                       WiFi.localIP() ) )            // Start DNS responder
    {
      dbgprint ( "Error setting up MDNS responder!" ) ;
    }
  #endif
  dbgprint ( "Start server for commands" ) ;
  // Specify handling of the various commands (case sensitive). The startpage will be returned if
  // no arguments are given.  The first argument has the format with "/?parameter=value".
  // Example: "/?volume=90"
  // Multiple commands are alowed, like "/?volume=95&station=+1"
  cmdserver.on ( "/", handleCmd ) ;                  // Handle startpage
  cmdserver.onNotFound ( handleFS ) ;                // Handle file from FS
  cmdserver.begin() ;
  delay ( 1000 ) ;                                   // Show IP for a wile
  connecttohost() ;                                  // Connect to the selected host
  ArduinoOTA.begin() ;                               // Allow update over the air
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
  mp3.setVolume ( reqvol ) ;                      // Set to requested volume
  ArduinoOTA.handle() ;                           // Check for OTA
}


//******************************************************************************************
//                              D I S P L A Y I N F O                                      *
//******************************************************************************************
// Show a string on the LCD at a specified y-position in a specified color                 *
//******************************************************************************************
void displayinfo ( const char *str, int pos, uint16_t color )
{
  tft.fillRect ( 0, pos, 160, 40, BLACK ) ;   // Clear the space for new info
  tft.setTextColor ( color ) ;                // Set the requested color
  tft.setCursor ( 0, pos ) ;                  // Prepare to show the info
  tft.print ( str ) ;                         // Show the string
}


//******************************************************************************************
//                        S H O W S T R E A M T I T L E                                    *
//******************************************************************************************
// show artist and songtitle if present in metadata                                        *
//******************************************************************************************
void showstreamtitle()
{
  char*       p1 ;
  char*       p2 ;

  if ( strstr ( metaline, "StreamTitle=" ) )
  {
    p1 = metaline + 12 ;                    // Begin of artist and title
    if ( p2 = strstr ( metaline, ";" ) )    // Search for end of title
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
  dbgprint ( metaline ) ;
  displayinfo ( streamtitle, 20, CYAN ) ;   // Show title at position 20
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
  
  switch ( datamode )
  {
    case DATA :                                        // Handle next byte of MP3 data
      buf[chunkcount++] = b ;                          // Save byte in cunkbuffer
      if ( chunkcount == sizeof(buf) )                 // Buffer full?
      {
        mp3.playChunk ( buf, chunkcount ) ;            // Yes, send to player
        chunkcount = 0 ;                               // Reset count
      }
      totalcount++ ;                                   // Count number of bytes, ignore overflow
      if ( --datacount == 0 )                          // End of datablock?
      {
        if ( chunkcount )                              // Yes, stil data in buffer?
        {
          mp3.playChunk ( buf, chunkcount ) ;          // Yes, send to player
          chunkcount = 0 ;                             // Reset count
        }
        datamode = METADATA ;
        firstmetabyte = true ;                         // Expecting first metabyte (counter)
      }
      break ;
    case INIT :                                        // Initialize for header receive
      LFcount = 0 ;                                    // For detection end of header
      bitrate = 0 ;                                    // Bitrate still unknown
      metaindex = 0 ;                                  // Prepare for new line
      datamode = HEADER ;                              // Handle header
      totalcount = 0 ;                                 // Reset totalcount
      // slip into HEADER handling, no break!
    case HEADER :                                      // Handle next byte of header 
      if ( ( b > 0x7F ) ||                             // Ignore unprintable characters
           ( b == '\r' ) ||                            // Ignore CR
           ( b == '\0' ) )                             // Ignore NULL
      {
        // Yes, ignore
      }
      else if ( b == '\n' )                            // Linefeed ?
      {
        LFcount++ ;                                    // Count linefeeds
        metaline[metaindex] = '\0' ;                   // Mark end of string
        metaindex = 0 ;                                // Reset for next line
        dbgprint ( metaline ) ;                        // Show it
        if ( strstr ( metaline, "icy-br:" ) == metaline )
        {
          // Found bitrate tag, read the bitrate
          bitrate = atoi ( metaline + 7 ) ;
        }
        else if ( strstr ( metaline, "icy-metaint:" ) == metaline )
        {
          // Found bitrate tag, read the bitrate
          metaint = atoi ( metaline + 12 ) ;
        }
        else if ( strstr ( metaline, "icy-name:" ) == metaline )
        {
          // Found station name, save it, prevent overflow
          strncpy ( sname, metaline + 9, sizeof ( sname ) ) ;
          sname[sizeof(sname)-1] = '\0' ;
          displayinfo ( sname, 60, YELLOW ) ;          // Show title at position 60
        }
        if ( bitrate && ( LFcount == 2 ) )
        {
          datamode = DATA ;                            // Expecting data
          datacount = metaint ;                        // Number of bytes before first metadata
          chunkcount = 0 ;                             // Reset chunkcount
          mp3.startSong() ;                            // Start a new song
        }
      }
      else
      {
        metaline[metaindex] = (char)b ;               // Normal character, put new char in metaline
        if ( metaindex < ( sizeof(metaline) - 2 ) )   // Prevent buffer overflow
        {
          metaindex++ ;
        }
        LFcount = 0 ;                                 // Reset double CRLF detection
      }
      break ;
    case METADATA :                                   // Handle next bye of metadata
      if ( firstmetabyte )                            // First byte of metadata?
      {
        firstmetabyte = false ;                       // Not the first anymore
        metacount = b * 16 + 1 ;                      // New count for metadata including length byte
        metaindex = 0 ;                               // Place to store metadata
        if ( metacount > 1 )
        {
          sprintf ( sbuf, "Metadata block %d bytes",
                    metacount-1 ) ;                  // Most of the time there are zero bytes of metadata
          dbgprint ( sbuf ) ;
        }
      }
      else
      {
        metaline[metaindex] = (char)b ;               // Normal character, put new char in metaline
        if ( metaindex < ( sizeof(metaline) - 2 ) )   // Prevent buffer overflow
        {
          metaindex++ ;
        }
      }
      if ( --metacount == 0 )                         
      {
        if ( metaindex )                              // Any info present?
        {
          metaline[metaindex] = '\0' ;
          // metaline contains artist and song name.  For example:
          // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
          // Sometimes it is just other info like:
          // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
          // Isolate the StreamTitle, remove leading and trailing quotes if present.
          showstreamtitle() ;                         // Show artist and title if present in metadata 
        }
        datacount = metaint ;                         // Reset data count
        chunkcount = 0 ;                              // Reset chunkcount
        datamode = DATA ;                             // Expecting data
      }
      break ;
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
  String fnam ;

  fnam = request->url() ;
  sprintf ( sbuf, "onFileRequest received %s", fnam.c_str() ) ;
  dbgprint ( sbuf ) ;
  request->send ( SPIFFS, fnam, getContentType ( fnam ) ) ;
}


//******************************************************************************************
//                             H A N D L E C M D                                           *
//******************************************************************************************
// Handling of the various commands from remote (case sensitive). All commands start with  *
// "/command", followed by "?parameter=value".  Example: "/command?volume=50".             *
// Examples with available parameters:                                                     *
//   volume     = 95                        // Percentage between 0 and 100                *
//   upvolume   = 2                         // Add percentage to current volume            *
//   downvolume = 2                         // Subtract percentage from current volume     *
//   preset     = 5                         // Select preset 5 station for listening       *
//   uppreset   = 1                         // Select next preset station for listening    *
//   downpreset = 1                         // Select previous preset station              *
//   station    = address:port              // Store new preset station and select it      *
//   delete     = 0                         // Delete current playing station              *
//   delete     = 5                         // Delete preset station number 5              *
//   status     = 0                         // Show current station:port                   *
//   test       = 0                         // For test purposes                           *
//   debug      = 0 or 1                    // Switch debugging on or off                  *
// Multiple parameters are allowed.  An extra command may be "version=<random number>" in  *
// to prevent browsers like Edge and IE to use their cache.  This "version" is ignored.    *
// Example: "/command?volume+=5&version=0.9775479450590543"                                *
//******************************************************************************************
void handleCmd ( AsyncWebServerRequest *request )
{
  AsyncWebParameter* p ;                                // Points to parameter structure
  String             argument ;                         // Next argument in command
  String             value ;                            // Value of an argument
  int                ivalue ;                           // Value of argument as an integer
  static char        reply[80] ;                        // Reply to client
  uint8_t            oldvol ;                           // Current volume
  uint8_t            newvol ;                           // Requested volume
  int                numargs ;                          // Number of arguments
  int                i ;                                // Loop through the commands
  bool               relative ;                         // Relative argument (+ or -)
  uint32_t           t ;                                // For time test
  
  t = millis() ;                                        // Timestamp at start
  numargs = request->params() ;                         // Get number of arguments
  if ( numargs == 0 )                                   // Any arguments
  {
    request->send ( SPIFFS, "/index.html",              // No parameters, send the startpage
                    "text/html" ) ;
    return ;
  }
  strcpy ( reply, "Command(s) accepted" ) ;             // Default reply
  for ( i = 0 ; i < numargs ; i++ )
  {
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
    sprintf ( sbuf, "Command: %s with parameter %s converted to %d",
              argument.c_str(), value.c_str(), ivalue ) ;
    dbgprint ( sbuf ) ;
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
      sprintf ( reply, "Volume is now %d", reqvol ) ;  // Reply new volume
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
        break ;
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
      sprintf ( reply, "Free memory is %d,  ringbuf %d, stream %d",
                system_get_free_heap_size(), rcount, mp3client.available() ) ;
    }
    else if ( argument.indexOf ( "version" ) == 0 )     // Random version number
    {
      // Simply ignore this argument.  Used to prevent cashing in browser
    }
    // To do: commands for bass/treble control
    else
    {
      sprintf ( reply, "Esp-radio called with ilegal parameter: %s",
                argument.c_str() ) ;
      break ;                                           // Stop interpreting
    }
  }                                                     // End of arguments loop
  request->send ( 200, "text/plain", reply ) ;          // Send the reply
  t = millis() - t ;
  sprintf ( sbuf, "Reply sent within %d msec", t ) ;
  dbgprint ( sbuf ) ;
  // If it takes too long to send a reply, we run into the "LmacRxBlk:1"-problem.
  // Reset the ESP8266..... 
  if ( t > 8000 )
  {
    ESP.restart() ;                                    // Last resource
  }
}

