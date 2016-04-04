//******************************************************************************************
//*  Esp_radio -- Webradio receiver for ESP8266, 1.8 color display and VS1053 MP3 module.  *
//*  With ESP8266 running at 80 MHz, it is capable of handling up to 256 kb bitrate.       *
//******************************************************************************************
// ESP8266 libraries used:
//  - SPI
//  - Adafruit_GFX
//  - TFT_ILI9163C
// A library for the VS1053 (for ESP8266) is not available (or not easy to find).  Therefore
// a class for this module is derived from the maniacbug library and integrated in this sketch.
//
// See http://www.internet-radio.com for suitable stations.  Add the staions of your choice
// to the table "hostlist" in the global data secting further on.
//
// Brief description of the program:
// First a suitable WiFi network is found and a connection is made.
// Then a connection will be made to a shoutcast server.  The server starts with some
// info in the header in readable ascii, ending with a double CRLF, like:
// icy-name:Classic Rock Florida - SHE Radio
// icy-genre:Classic Rock 60s 70s 80s Oldies Miami South Florida
// icy-url:http://www.ClassicRockFLorida.com
// content-type:audio/mpeg
// icy-pub:1
// icy-metaint:32768          - Metadata after 32768 bytes of MP3-data
// icy-br:128                 - in kb/sec 
//
// After de double CRLF is received, the server starts sending mp3-data.  This data contains
// metadata (non mp3) after every "metaint" mp3 bytes.  This metadata is empty in most cases,
// but if any is available the content will be presented on the TFT.
// Pushing the input button causes the player to select the next station in the hostlist.
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
// -------  ------  --------------  ---------------     -------------------  -------------------
// D0       GPIO16  16              -                   pin 1 DCS            -
// D1       GPIO5   5               -                   pin 2 CS             LED on nodeMCU
// D2       GPIO4   4               -                   pin 4 DREQ           -
// D3       GPIO0   0  FLASH        -                   -                    Control button
// D4       GPIO2   2               pin 3 (D/C)         -                    -
// D5       GPIO14  14 SCLK         pin 5 (CLK)         pin 5 SCK            -
// D6       GPIO12  12 MISO         -                   pin 7 MISO           -
// D7       GPIO13  13 MOSI         pin 4 (DIN)         pin 6 MOSI           -
// D8       GPIO15  15              pin 2 (CS)          -                    -
// D9       GPI03   3 RXD0          -                   -                    Reserved for serial input
// D10      GPIO1   1 TXD0          -                   -                    Reserved for serial output
// VCC      -       -               pin 6 (VCC)         -                    -
// RST      -       -               pin 1 (RST)         pin 3 RESET          -
//
// 31-03-2016, ES: First set-up.
// 01-04-2016, ES: Detect missing VS1053 at start-up.
//
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <Ticker.h>
#include <stdio.h>
#include <string.h>
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
//
//******************************************************************************************

//******************************************************************************************
// VS1053 stuff.  Based on maniacbug library.                                              *
//******************************************************************************************
// VS1053 class definition.                                                                *
//******************************************************************************************
class VS1053
{
private:
  uint8_t cs_pin ;                        //**< Pin where CS line is connected
  uint8_t dcs_pin ;                       //**< Pin where DCS line is connected
  uint8_t dreq_pin ;                      //**< Pin where DREQ line is connected
  const uint8_t vs1053_chunk_size = 32 ;
  // SCI Register
  const uint8_t SCI_MODE          = 0x0 ;
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
  const uint8_t SM_SDINEW         = 11 ;  // Bitnumber in SCI_MODE
  const uint8_t SM_RESET          = 2 ;   // Bitnumber in SCI_MODE
  const uint8_t SM_TESTS          = 5 ;   // Bitnumber in SCI_MODE for tests
  const uint8_t SM_LINE1          = 14 ;  // Bitnumber in SCI_MODE for Line input
  SPISettings VS1053_SPI ;                // SPI settings for this slave
protected:
  inline void await_data_request() const
  {
    while ( !digitalRead ( dreq_pin ) ) ;
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
  void     sdi_send_zeroes ( size_t length ) ;
  void     wram_write ( uint16_t address, uint16_t data ) ;

public:
  // Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
  VS1053 ( uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin ) ;
  void begin() ;                                 // Begin operation.  Sets pins correctly,
                                                 // and prepares SPI bus.
  void startSong() ;                             // Prepare to start playing. Call this each
                                                 // time a new song starts.
  void playChunk ( uint8_t* data, size_t len ) ; //Play a chunk of data.  Copies the data to
                                                 // the chip.  Blocks until complete.
  void stopSong() ;                              // Finish playing a song. Call this after
                                                 // the last playChunk call.
  void setVolume ( uint8_t vol ) const ;         // Set the player volume.Level from 0-255,
                                                 // lower is louder.
  //Print configuration details to serial output.
  //void printDetails ( const char *header ) const ;
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
  SPI.write ( 2 ) ;                     // Write operation
  SPI.write ( _reg ) ;                  // Register to write (0..0xF)
  SPI.write16 ( _value ) ;              // Send 16 bits data  
  await_data_request() ;
  control_mode_off() ;
}

void VS1053::sdi_send_buffer ( uint8_t* data, size_t len )
{
  size_t chunk_length ;                 // Length of chunk 32 byte or shorter
  
  data_mode_on() ;
  while ( len )                         // More to do?
  {
    await_data_request() ;              // Wait for space available
    chunk_length = min ( len, vs1053_chunk_size ) ;
    len -= chunk_length ;
    SPI.writeBytes ( data, chunk_length ) ;
    data += chunk_length ;
  }
  data_mode_off() ;
}

void VS1053::sdi_send_zeroes ( size_t len )
{
  data_mode_on() ;
  while ( len )
  {
    await_data_request() ;
    size_t chunk_length = min ( len, vs1053_chunk_size ) ;
    len -= chunk_length ;
    while ( chunk_length-- )
    {
      SPI.write ( 0 ) ;
    }
  }
  data_mode_off();
}

void VS1053::wram_write ( uint16_t address, uint16_t data )
{
  write_register ( SCI_WRAMADDR, address ) ;
  write_register ( SCI_WRAM, data ) ;
}

void VS1053::begin()
{
  uint16_t  i, r, r2, cnt = 0 ;
  char      sbuf[60] ;                     // For debugging

  
  pinMode      ( cs_pin,    OUTPUT ) ;      // The SCI and SDI will start deselected
  digitalWrite ( cs_pin,    HIGH ) ;
  pinMode      ( dcs_pin,   OUTPUT ) ;
  digitalWrite ( dcs_pin,   HIGH ) ;
  pinMode      ( dreq_pin,  INPUT ) ;       // DREQ is an input
  //delay ( 10 ) ;
  // Init SPI in slow mode ( 1 MHz )
  VS1053_SPI = SPISettings ( 1000000, MSBFIRST, SPI_MODE0 ) ;
  //printDetails ( "Right after reset/startup" ) ;
  delay ( 20 ) ;
  //printDetails ( "20 msec after reset" ) ;
  // If DREQ is low, there is problably no VS1053 connected.  Pull the line HIGH
  // in order to prevent an endless loop waiting for this signal.  The rest of the
  // software will still work, but readbacks from VS1053 will fail.
  if ( !digitalRead ( dreq_pin ) )
  {
    dbgprint ( "VS1053 not properly installed!" ) ;
    pinMode ( dreq_pin,  INPUT_PULLUP ) ;     // DREQ is now input with pull-up
  }
  // TESTING.  Check if SCI bus can write and read without errors.
  // Will give warnings on serial output if DEBUG is active.
  // A maximum of 20 errors will be reported.
  for ( i = 0 ; ( i < 0xFFFF ) && ( cnt < 20 ) ; i += 3 )
  {
    write_register ( SCI_VOL, i ) ;           // Write data to SCI_VOL
    r = read_register ( SCI_VOL ) ;           // Read back for the first time
    r2 = read_register ( SCI_VOL ) ;          // Raed back a second time
    if  ( r != r2 )                           // Check for 2 equal reads
    {
      sprintf ( sbuf, "VS1053 error retry SB:%04X R1:%04X R2:%04X", i, r, r2 ) ;
      dbgprint ( sbuf ) ;
      cnt++ ;
      delay ( 10 ) ;
    }
    if  ( i != r )                            // Check for the right data
    {
      sprintf ( sbuf, "VS1053 error reading back SB:%04X IS:%04X", i, r ) ;
      dbgprint ( sbuf ) ;
      cnt++ ;
      delay ( 10 ) ;
    }
    yield() ;                                 // Allow ESP firmware to do some bookkeeping
  }
  // Most VS1053 modules will start up in midi mode.  The result is that there is no audio
  // when playing MP3.  You can modify the board, but there is a more elegant way:
  wram_write ( 0xC017, 3 ) ;                  // Switch to MP3 mode
  wram_write ( 0xC019, 0 ) ;                  // Switch to MP3 mode
  delay ( 100 ) ;
  //printDetails ( "After test loop" ) ;
  //SoftReset
  write_register ( SCI_MODE, _BV ( SM_SDINEW ) | _BV ( SM_RESET ) ) ;
  delay ( 10 ) ;
  //printDetails ( "10 msec after soft reset" ) ;
  // Switch on the analog parts
  write_register ( SCI_AUDATA, 44100+1 ) ;    // 44.1kHz + stereo
  write_register ( SCI_CLOCKF, 6 << 13 | 3 << 11 ) ;   // Initial clock settings
  write_register ( SCI_MODE, _BV ( SM_SDINEW ) | _BV ( SM_LINE1 ) ) ;
  setVolume ( 40 ) ;                          // Normal volume
  delay ( 10 ) ;
  await_data_request() ;
  //write_register ( SCI_CLOCKF, 0xB800 ) ;   // Experimenting with higher clock settings
  delay ( 1 ) ;
  await_data_request() ;
  //SPISetFastClock. Now you can set high speed SPI clock.
  VS1053_SPI = SPISettings ( 8000000, MSBFIRST, SPI_MODE0 ) ;
  //printDetails ( "After last clocksetting, idling" ) ;
  delay (100) ;
}

void VS1053::setVolume ( uint8_t vol ) const
{
  // Set volume.  Both left and right.
  uint16_t value = vol ;

  value <<= 8 ;
  value |= vol ;
  write_register ( SCI_VOL, value ) ; // VOL
}

void VS1053::startSong()
{
  sdi_send_zeroes ( 10 ) ;
}

void VS1053::playChunk ( uint8_t* data, size_t len )
{
  sdi_send_buffer ( data, len ) ;
}

void VS1053::stopSong()
{
  sdi_send_zeroes ( 2048 ) ;
}


/* printDetails commented out for now, but can be used for debugging.
void VS1053::printDetails ( const char *header ) const
{
  char         sbuf[60] ;                // For debugging
  unsigned int regbuf[16] ;
  int          i ;

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
    sprintf ( sbuf, "%3X - %5X", i, regbuf[i] ) ;
    dbgprint ( sbuf ) ;
  }
  dbgprint ( sbuf ) ;
}
*/

//******************************************************************************************
// End VS1053 stuff.                                                                       *
//******************************************************************************************



//******************************************************************************************
// Global data section.                                                                    *
//******************************************************************************************
enum datamode_t { INIT, HEADER, DATA, METADATA } ;  // State for datastream
// Global variables
//  Configure the next 2 lines for your WiFi network(s)
const char*   networks[]  = { "NETW-01",  "NETW-02", "NETW-03 } ;
const char*   passwords[] = { "PW-01",    "PW-02",   "PW-03" } ;
char          ssid[33] ;                // SSID of selected WiFi network
char          password[33] ;            // Password for selected WiFi network
const int     DEBUG = 1 ;
WiFiClient    client ;                  // An instance of the client
TFT_ILI9163C  tft = TFT_ILI9163C ( TFT_CS, TFT_DC ) ;
Ticker        tckr ;                    // For timing 10 sec
uint32_t      totalcount = 0 ;          // Counter mp3 data
char          sbuf[100] ;               // Voor debug regels
datamode_t    datamode ;                // State of datastream
int           metacount ;               // Number of bytes in metadata
int           datacount ;               // Counter databytes before metadata
char          metaline[150] ;           // Readable line in metadata
char          streamtitle[150] ;        // Streamtitle from metadata
int           bitrate = 0 ;             // Bitrate in kb/sec            
int           metaint = 0 ;             // Number of databytes between metadata
const char*   hostlist[] = { "109.206.96.34:8100",
                             "us1.internet-radio.com:8180",
                             "us2.internet-radio.com:8050",
                             "us1.internet-radio.com:15919",
                             "us2.internet-radio.com:8132",
                             "us1.internet-radio.com:8105",
                             "205.164.36.153:80",	// BOM PSYTRANCE (1.FM TM)  64-kbps
                             "205.164.62.15:10032",	// 1.FM - GAIA, 64-kbps
                             "109.206.96.11:80",	// TOP FM Beograd 106,8  64-kpbs
                             "85.17.121.216:8468",	// RADIO LEHOVO 971 GREECE, 64-kbps
                             "85.17.121.103:8800",	// STAR FM 88.8 Corfu Greece, 64-kbps
                             "85.17.122.39:8530",	// www.stylfm.gr laiko, 64-kbps
                             "144.76.204.149:9940",	// RADIO KARDOYLA - 64-kbps 22050 Hz
                             "198.50.101.130:8245",	// La Hit Radio, Rock - Metal - Hard Rock, 32	
                             "94.23.66.155:8106",	// *ILR CHILL & GROOVE* 64-kbps
                             "205.164.62.22:7012",	// 1.FM - ABSOLUTE TRANCE (EURO) RADIO   64-kbps
                             "205.164.62.13:10144",	// 1.FM - Sax4Ever   64-kbps 
                             "83.170.104.91:31265",	// Paradise Radio 106   64-kbps
                             "205.164.62.13:10152",	// Costa Del Mar - Chillout (1.FM), 64-kbps
                             "46.28.48.140:9998", 	// AutoDJ, latin, cumbia, salsa, merengue, regueton, pasillos , 48-kbps
                             "50.7.173.162:8116", 	// Big B Radio #CPOP - Asian Music - 64k
                             "50.7.173.162:8097", 	// Big B Radio #AsianPop - 64kbps
                             "195.154.167.62:7264", 	// Radio China - 48kbps
                             "198.154.106.104:8985",	// radioICAST.com Acid Jazz Blues Rock 96K
                             NULL } ;
int           hostindex = 0 ;           // Index in hostlist
char          host[100] ;               // The hostname
char          name[100] ;               // Stationname
int           port ;                    // Port number for host
bool          buttonpushed = false ;    // Select button pushed
// The object for the MP3 player
VS1053        mp3 (  VS1053_CS, VS1053_DCS, VS1053_DREQ ) ;


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
//******************************************************************************************
void listNetworks()
{
  int         max = -1000 ;   // Used for searching strongest WiFi signal
  int         newstrength ;
  const char* acceptable ;    // Netwerk is acceptable for connection
  int         i, j ;
  bool        found ;         // True if acceptable network found
  
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
  found = false ;
  for ( i = 0 ; i < numSsid ; i++ )
  {
    acceptable = "" ;                                    // Assume not acceptable
    for ( j = 0 ; j < sizeof ( networks ) / sizeof ( networks[0] ) ; j++ )
    {
      // Check if this network is acceptable
      if ( strcmp ( networks[j], WiFi.SSID ( i ) ) == 0 )
      {
        acceptable = "Acceptable" ;
        found = true ;                                   // Acceptable network found
        break ;
      }
    }
    sprintf ( sbuf, "%2d) %-25s Signal: %3d dBm Encryption %4s  %s",
                   i + 1, WiFi.SSID ( i ), WiFi.RSSI ( i ),
                   getEncryptionType ( WiFi.encryptionType ( i ) ),
                   acceptable ) ;
    dbgprint ( sbuf ) ;
    newstrength = WiFi.RSSI ( i ) ;
    if ( found )
    {
      if ( newstrength > max )                           // This is a better Wifi
      {
        max = newstrength ;
        strcpy ( ssid, WiFi.SSID ( i ) ) ;               // Remember name and password
        strcpy ( password, passwords[j] ) ;
      }
    }
  }
  dbgprint ( "--------------------------------------" ) ;
  sprintf ( sbuf, "Selected network: %-25s", ssid ) ;
  dbgprint ( sbuf ) ;
}


//******************************************************************************************
//                             G E T E N C R Y P T I O N T Y P E                           *
//******************************************************************************************
// Read the encryption type of the network and return as a 4 byte name                     *
//******************************************************************************************
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
  static uint32_t oldtotalcount = 7321 ;
  
  if ( totalcount == oldtotalcount )
  {
    // No data detected!
    dbgprint ( "No data input" ) ;
    while ( true )
    {
      // Let the watchdog time out....
    }
  }
  oldtotalcount = totalcount ;                // Save for comparison in next cycle
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
        buttonpushed = true ;                 // Remember action
        //dbgprint ( "Button pushed" ) ;
      }
    }
  }
}


//******************************************************************************************
//                            C O N N E C T T O H O S T                                    *
//******************************************************************************************
// Connect to the Internet radio server                                                    *
//******************************************************************************************
void connecttohost()
{
  char* p ;

  if ( client.connected() )
  {
    dbgprint ( "Stop client" ) ;
    client.flush() ;
    client.stop() ;
  }
  displayinfo ( "   ** Internet radio **", 0, WHITE ) ;
  strcpy ( host, hostlist[hostindex] ) ;      // Select first station number
  p = strstr ( host, ":" ) ;                  // Search for separator
  *p++ = '\0' ;                               // Remove port from string and point to port
  port = atoi ( p ) ;                         // Get portnumber as integer
  sprintf ( sbuf, "Connect to %s on port %d",
            host, port ) ;
  dbgprint ( sbuf ) ;
  displayinfo ( sbuf, 60, YELLOW ) ;          // Show info at position 60
  delay ( 2000 ) ;                            // Show for some time
  client.flush() ;
  if ( client.connect ( host, port ) )
  {
    dbgprint ( "Connected to server" ) ;
    // This will send the request to the server. Request metadata.
    client.print ( String ( "GET / HTTP/1.1\r\n" ) +
               "Host: " + host + "\r\n" +
               "Icy-MetaData:1\r\n" +
               "Connection: close\r\n\r\n");
  }
  datamode = INIT ;                           // Start in metamode
}


//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup()
{
  char        buf[50] ;
  byte        mac[6] ;
  int         i ;
  uint32_t    freemem ;

  freemem = system_get_free_heap_size() ;
  Serial.begin ( 115200 ) ;                   // For debug
  sprintf ( sbuf, "\nStarting...  Free memory %d", freemem ) ;
  dbgprint ( sbuf ) ;
  SPI.begin() ;                               // Init SPI bus
  pinMode ( BUTTON, INPUT ) ;                 // Input for control button
  mp3.begin() ;                               // Initialize VS1053 player
  tft.begin() ;                               // Init TFT interface
  tft.fillRect ( 0, 0, 160, 128, BLACK ) ;    // Clear screen does not work when rotated
  tft.setRotation ( 3 ) ;                     // Use landscape format
  tft.clearScreen() ;                         // Clear screen
  tft.setTextSize ( 1 ) ;                     // Small character font
  tft.setTextColor ( WHITE ) ;  
  tft.println ( "Starting" ) ;
  delay(10);
  streamtitle[0] = '\0' ;                     // No title yet
  tckr.attach ( 0.100, timer100 ) ;           // Every 100 msec
  listNetworks() ;                            // Search for strongest WiFi network
  // Connect to WiFi network
  WiFi.begin ( ssid, password ) ;             // Connect to selected SSID
  while ( WiFi.status() != WL_CONNECTED )
  {
    sprintf ( sbuf, "Try WiFi %s", ssid ) ;
    dbgprint ( sbuf ) ;
    tft.println ( sbuf ) ;
    delay ( 1500 ) ;
  }
  sprintf ( sbuf, "IP = %d.%d.%d.%d",
                  WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] ) ;
  dbgprint ( sbuf ) ;
  tft.println ( sbuf ) ;
  connecttohost() ;                           // Connect to the selected host
}


//******************************************************************************************
//                                   L O O P                                               *
//******************************************************************************************
// Main loop of the program.                                                               *
// A connection to an MP3 server is active and we are ready to receive data.               *
//******************************************************************************************
void loop()
{
  if ( client.available() )                   // Any data in input stream?
  {
    handlebyte ( client.read() ) ;            // Yes, handle it
  }  
  if ( buttonpushed )                         // New station requested?
  {
    buttonpushed = false ;                    // Reset request
    mp3.stopSong() ;                          // Stop playing
    hostindex++ ;                             // Select next channel
    if ( hostlist[hostindex] == NULL )        // End of list?
    {
      hostindex = 0 ;                         // Yes, back to channel 0
    }
    connecttohost() ;                         // Switch to new host
  }
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
// The buffer is defined as an array of 32 bits values to allow faster SPI.                *
//******************************************************************************************
void handlebyte ( uint8_t b )
{
  static int      metaindex ;                          // Index in metaline
  static int      metablock ;                          // Count number of metadata seen
  static bool     firstmetabyte ;                      // True if first metabyte (counter) 
  static int      LFcount ;                            // Detection of end of header
  static int      mp3cnt ;                             // Number of byte in buffer
  static uint32_t mp3buf[32] ;                         // Buffer for player
  static uint8_t  *dptr ;                              // Points within mp3buf
  
  switch ( datamode )
  {
    case DATA :                                        // Handle next byte of MP3 data
      mp3.playChunk ( &b, 1 ) ;                        // Send to player, very short chunk
      if ( --datacount == 0 )                          // End of datablock, reset datamode
      {
        totalcount++ ;                                 // Count number of bytes
        datamode = METADATA ;
        firstmetabyte = true ;                         // Expecting first metabyte (counter)
      }
      break ;
    case INIT :                                        // Initialize for header receive
      LFcount = 0 ;                                    // For detection end of header
      bitrate = 0 ;                                    // Bitrate still unknown
      metaindex = 0 ;                                  // Prepare for new line
      datamode = HEADER ;                              // Handle header
      metablock = 0 ;                                  // Reset debug counter
      totalcount = 0 ;                                 // Reset totalcount
      // slip into HEADER handling, no break!
    case HEADER :                                      // Handle next byte of header 
      if ( ( b > 0x7f ) ||                             // Ignore unprintable characters
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
          strncpy ( name, metaline + 9, sizeof ( name ) ) ;
          name[sizeof(name)-1] = '\0' ;
          displayinfo ( name, 60, YELLOW ) ;           // Show title at position 60
        }
        if ( bitrate && ( LFcount == 2 ) )
        {
          datamode = DATA ;                            // Expecting data
          datacount = metaint ;                        // Number of bytes before first metadata
          dptr = (uint8_t*)mp3buf ;                    // Prepare for new mp3 data
          mp3cnt = 0 ;       
          mp3.startSong() ;                            // Start a new program
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
          sprintf ( sbuf, "Metadata block %d, %d bytes",
                    metablock, metacount-1 ) ;        // Most of the time there are zero bytes of metadata
          dbgprint ( sbuf ) ;
        }
        metablock = ++metablock % 10000 ;             // Count num for debug
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
          showstreamtitle() ;                        // Show artist and title if present in metadata 
        }
        datacount = metaint ;                        // Reset data count
        datamode = DATA ;                            // Expecting data
      }
      break ;
  }
}
