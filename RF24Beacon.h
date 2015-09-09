// RF24Beacon - Library

#ifndef __RF24BEACON_H__
#define __RF24BEACON_H__

#ifdef ARDUINO
    #if ARDUINO < 100
        #include <WProgram.h>
    #else
        #include <Arduino.h>
    #endif
#else
    #include "./libs/Arduino.h"
#endif

#include <stddef.h>

#ifdef ARDUINO
    #include <SPI.h>
#else
    #include "./libs/SPI.h"
    #include <inttypes.h>
#endif
#include "nRF24L01.h"

class RF24Beacon {
private:
    // nRF24L01+ connections
    uint8_t cePin;          // CE pin number
    uint8_t csnPin;         // CSN pin number

    // Beacon settings
    uint8_t macAddress[6];
    uint8_t *name, nameLen;
    uint8_t *data, dataLen, spaceForData;
    uint32_t advInterval;   // advertizing interval(minimum)

    // data packet
    uint8_t buf[32];
    uint8_t pkt[32];

    // for channel hopping
    uint8_t ch;

    // hardware access functions
    uint8_t spi_byte( uint8_t byte );
    void nrf_cmd( uint8_t cmd, uint8_t data );
    void nrf_simplebyte( uint8_t cmd );
    void nrf_manybytes( uint8_t *data, uint8_t len );

public:

    RF24Beacon( uint8_t _ce, uint8_t _csn );

    void begin( void );
    void setMAC( uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t );
    uint8_t setName( const char* );
    uint8_t setData( const void*, uint8_t );
    uint8_t sendData( const void*, uint8_t );
    void beep( void );  // to send one beacon packet
    void end( void );

};

// utility functions
uint8_t swapbits( uint8_t );
void btLeCrc( const uint8_t* data, uint8_t len, uint8_t* dst );
void btLeWhiten( uint8_t* data, uint8_t len, uint8_t whitenCoeff );
uint8_t btLeWhitenStart( uint8_t chan );
void btLePacketEncode( uint8_t* packet, uint8_t len, uint8_t chan );

#endif