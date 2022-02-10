//******************************************************************************************
// Header file for SPI RAM routines.                                                       *
//******************************************************************************************

#ifndef _SPIRAM_HPP
  #define SRAM_CS        10                   // GPIO1O CS pin
  #define SRAM_FREQ    13e6                   // The 23LC1024 supports theorically up to 20MHz

  bool spaceAvailable() ;
  uint16_t dataAvailable() ;
  uint16_t getFreeBufferSpace() ;
  void bufferWrite ( uint8_t *b ) ;
  void bufferRead ( uint8_t *b ) ;
  void bufferReset() ;
  void spiramSetup() ;
  #define _SPIRAM_HPP
#endif