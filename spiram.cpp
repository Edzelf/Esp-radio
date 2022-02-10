//******************************************************************************************
// SPI RAM routines.                                                                       *
//******************************************************************************************
// Use SPI RAM as a circular buffer with chunks of 32 bytes.                               *
//******************************************************************************************

#include <ESP8266Spiram.h>                  // https://github.com/Gianbacchio/ESP8266_Spiram

#define SRAM_SIZE  131072                   // Total size SPI ram in bytes
#define CHUNKSIZE      32                   // Chunk size
#define SRAM_CH_SIZE 4096                   // Total size SPI ram in chunks

#define SRAM_CS        10                   // GPIO1O SRAM CS pin
#define SRAM_FREQ    16e6                   // The 23LC1024 supports theorically up to 20MHz

// SRAM opcodes
#define SRAM_READ    0x03
#define SRAM_WRITE   0x02

// Global variables
uint16_t   chcount ;                       // Number of chunks currently in buffer
uint32_t   readinx ;                       // Read index
uint32_t   writeinx ;                      // write index

ESP8266Spiram spiram ( SRAM_CS, SRAM_FREQ ) ;


//******************************************************************************************
//                              S P A C E A V A I L A B L E                                *
//******************************************************************************************
// Returns true if bufferspace is available.                                               *
//******************************************************************************************
bool spaceAvailable()
{
  return ( chcount < SRAM_CH_SIZE ) ;
}


//******************************************************************************************
//                              D A T A A V A I L A B L E                                  *
//******************************************************************************************
// Returns the number of chunks available in the buffer.                                   *
//******************************************************************************************
uint16_t dataAvailable()
{
  return chcount ;
}


//******************************************************************************************
//                    G E T F R E E B U F F E R S P A C E                                  *
//******************************************************************************************
// Return the free buffer space in chunks.                                                 *
//******************************************************************************************
uint16_t getFreeBufferSpace()
{
  return ( SRAM_CH_SIZE - chcount ) ;                   // Return number of chunks available
}


//******************************************************************************************
//                             B U F F E R W R I T E                                       *
//******************************************************************************************
// Write one chunk (32 bytes) to SPI RAM.                                                  *
// No check on available space.  See spaceAvailable().                                     *
//******************************************************************************************
void bufferWrite ( uint8_t *b )
{
  spiram.write ( writeinx * CHUNKSIZE, b, CHUNKSIZE ) ; // Put byte in SRAM
  writeinx = ( writeinx + 1 ) % SRAM_CH_SIZE ;          // Increment and wrap if necessary
  chcount++ ;                                           // Count number of chunks
}


//******************************************************************************************
//                             B U F F E R R E A D                                         *
//******************************************************************************************
// Read one chunk in the user buffer.                                                      *
// Assume there is always something in the bufferpace.  See dataAvailable()                *
//******************************************************************************************
void bufferRead ( uint8_t *b )
{
  spiram.read ( readinx * CHUNKSIZE, b, CHUNKSIZE ) ;   // return next chunk
  readinx = ( readinx + 1 ) % SRAM_CH_SIZE ;            // Increment and wrap if necessary
  chcount-- ;                                           // Count is now one less
}


//******************************************************************************************
//                            B U F F E R R E S E T                                        *
//******************************************************************************************
void bufferReset()
{
  readinx = 0 ;                                         // Reset ringbuffer administration
  writeinx = 0 ;
  chcount = 0 ;
}


//******************************************************************************************
//                                S P I R A M S E T U P                                    *
//******************************************************************************************
void spiramSetup()
{
  spiram.begin() ;                                  // Init ESP8266Spiram
  bufferReset() ;                                   // Reset ringbuffer administration
}
