// RF24Beacon.cpp

#include "RF24Beacon.h"

// This is a rather convoluted hack to extract the month number from the build date in
// the __DATE__ macro using a small hash function + lookup table. Since all inputs are
// const, this can be fully resolved by the compiler and saves over 200 bytes of code.
#define month(m) month_lookup[ (( ((( (m[0] % 24) * 13) + m[1]) % 24) * 13) + m[2]) % 24 ]
const uint8_t month_lookup[24] = { 0,6,0,4,0,1,0,17,0,8,0,0,3,0,0,0,18,2,16,5,9,0,1,7 };

static const uint8_t chRf[] = {  2, 26, 80 };
static const uint8_t chLe[] = { 37, 38, 39 };

// Public Functions
RF24Beacon::RF24Beacon( uint8_t _ce, uint8_t _csn ) {
    cePin   = _ce;
    csnPin  = _csn;

    macAddress[5] = 0x01;
    macAddress[4] = 0x23;
    macAddress[3] = 0x45;
    macAddress[2] = 0x67;
    macAddress[1] = 0x89;
    macAddress[0] = 0xAB;

    advInterval = 0;
    ch = 0;

    nameLen         = 0;
    dataLen         = 0;
    spaceForData    = 0;
    name = NULL;
    data = NULL;
}

void RF24Beacon::begin( void ) {

    pinMode( cePin, OUTPUT );
    pinMode( csnPin, OUTPUT );
    pinMode( 11, OUTPUT );
    pinMode( 13, OUTPUT );

    digitalWrite( csnPin, HIGH );
    digitalWrite( cePin, LOW );

    SPI.begin();
    SPI.setBitOrder( MSBFIRST );

    // initialize nRF24L01+
    nrf_cmd( 0x20, 0x12 );  // ON, no CRC, int on RX/TX done
    nrf_cmd( 0x21, 0x00 );  // no auto-ackowledge
    nrf_cmd( 0x22, 0x00 );  // no RX
    nrf_cmd( 0x23, 0x02 );  // 5B address
    nrf_cmd( 0x24, 0x00 );  // no auto-retransmit
    nrf_cmd( 0x26, 0x06 );  // 1MBPS at 0dB
    nrf_cmd( 0x27, 0x3E );  // clear various flags
    nrf_cmd( 0x3C, 0x00 );  // no dynamic payload
    nrf_cmd( 0x3D, 0x00 );  // no features
    nrf_cmd( 0x31, 0x20 );  // 32B payload
    nrf_cmd( 0x22, 0x01 );  // RX on pipe 0

    // BLE set address
    buf[0] = 0x30;
    buf[1] = swapbits( 0x8E );
    buf[2] = swapbits( 0x89 );
    buf[3] = swapbits( 0xBE );
    buf[4] = swapbits( 0xD6 );
    nrf_manybytes( buf, 5 );

    buf[0] = 0x2A;
    nrf_manybytes( buf, 5 );

    // Now clear the buffer and fill it with frame data
    for( uint8_t i = 0; i < 32; i++ )
        buf[i] = 0x00;
    buf[0] = 0x42;  // PDU
    buf[1] = 0x00;  // length - we'll update this later

    // set some random MAC address
    macAddress[0] = ((__TIME__[6]-0x30) << 4) | (__TIME__[7]-0x30);
    macAddress[1] = ((__TIME__[3]-0x30) << 4) | (__TIME__[4]-0x30);
    macAddress[2] = ((__TIME__[0]-0x30) << 4) | (__TIME__[1]-0x30);
    macAddress[3] = ((__DATE__[4]-0x30) << 4) | (__DATE__[5]-0x30);
    macAddress[4] = month(__DATE__);
    macAddress[5] = ((__DATE__[9]-0x30) << 4) | (__DATE__[10]-0x30);

    // MAC starts from 3rd byte of payload
    buf[2] = macAddress[5];
    buf[3] = macAddress[4];
    buf[4] = macAddress[3];
    buf[5] = macAddress[2];
    buf[6] = macAddress[1];
    buf[7] = macAddress[0];

    // LE-only , limited discovery
    buf[8] = 0x02;
    buf[9] = 0x01;
    buf[10] = 0x05;

    // printf("At begin() :\n");
    // displayPacket( buf );

}

void RF24Beacon::setMAC( uint8_t mac5, uint8_t mac4, uint8_t mac3,
                         uint8_t mac2, uint8_t mac1, uint8_t mac0 ) {

    macAddress[5] = mac5;
    macAddress[4] = mac4;
    macAddress[3] = mac3;
    macAddress[2] = mac2;
    macAddress[1] = mac1;
    macAddress[0] = mac0;

    // MAC starts from 3rd byte of payload
    buf[2] = macAddress[0];
    buf[3] = macAddress[1];
    buf[4] = macAddress[2];
    buf[5] = macAddress[3];
    buf[6] = macAddress[4];
    buf[7] = macAddress[5];

    // printf("After setMAC : \n");
    // displayPacket( buf );
}

uint8_t RF24Beacon::setName( const char* _name ) {

    uint8_t i = 0;
    nameLen = strlen(_name);

    buf[11] = nameLen+1;
    buf[12] = 0x08;
    for( i = 0; i < nameLen; i++ ) {
        buf[13+i] = _name[i];
    }
    name = &buf[13];     // start of name field

    // clear data field
    for( i = 12+nameLen+1; i < 32; i++ )
        buf[i] = 0x00;

    // printf("After setName : \n");
    // displayPacket( buf );

    spaceForData = 32         // total payload size
                   -11        // pdu+length+mac+settings+nameLen
                   -nameLen   // actual name
                   -2         // data length header
                   -3;      // CRC length

    // Serial.print( "NameLen = " ); Serial.println( nameLen );

    return ( spaceForData );
}

uint8_t RF24Beacon::setData( const void* bytes, uint8_t len ) {

    if( spaceForData < len ) {
        return 0;   // ERROR : data cannot be set
    }

    const uint8_t *buffer = reinterpret_cast<const uint8_t*>(bytes);

    dataLen = len;
    buf[10+nameLen+1+2]   = dataLen+1;
    buf[10+nameLen+1+1+2] = 0xFF;
    data = &buf[10+nameLen+1+1+1+2];

    for( uint8_t i = 0; i < dataLen; i++ ) {
        *(data+i) = buffer[i];
    }

    return 1;
}

void RF24Beacon::beep( void ) {

    uint8_t pktLen = 0, i = 0;
    // uint8_t pkt[32];

    // calculate packet length
    pktLen = 10+nameLen+1+1+dataLen+2;
    // update length field
    buf[1] = pktLen-1;
    // put dummy CRC
    buf[pktLen+1] = 0x55;
    buf[pktLen+2] = 0x55;
    buf[pktLen+3] = 0x55;

    // copy from buffer to packet
    for( i = 0; i < 32; i++ )
        pkt[i] = buf[i];

    // Channel hopping
    if( ++ch == sizeof(chRf) )
        ch = 0;    
    nrf_cmd( 0x25, chRf[ch] );
    nrf_cmd( 0x27, 0x6E );    // Clear flags

#ifndef ARDUINO
    printf("Before encoding(%02d) : ", 1+pktLen+3 );
    for( i = 0; i < 1+pktLen+3; i++ )
        printf("%02X ", pkt[i] );
    printf( "\n" );
// #else
    // Serial.print("Before encoding : "); Serial.print( 1+pktLen+3 ); Serial.print( "   " );
    // for( i = 0; i < 1+pktLen+3; i++ ) {
    //     Serial.print( pkt[i], HEX );
    //     Serial.print( " " );
    // }
    // Serial.println();
#endif
        
    btLePacketEncode( pkt, 1+pktLen+3, chLe[ch] );

#ifndef ARDUINO
    printf("After encoding(%02d)  : ", 1+pktLen+3 );
    for( i = 0; i < 1+pktLen+3; i++ )
        printf("%02X ", pkt[i] );
    printf( "\n" );
// #else
//     Serial.print("After encoding  : "); Serial.print( 1+pktLen+3 ); Serial.print( "   " );
//     for( i = 0; i < 1+pktLen+3; i++ ) {
//         Serial.print( pkt[i], HEX );
//         Serial.print( " " );
//     }
//     Serial.println();
#endif
    
    nrf_simplebyte( 0xE2 ); //Clear RX Fifo
    nrf_simplebyte( 0xE1 ); //Clear TX Fifo
    digitalWrite( csnPin, LOW );
    spi_byte( 0xA0 );
    for( i = 0 ; i < 1+pktLen+3 ; i++)
        spi_byte( pkt[i] );
    digitalWrite( csnPin, HIGH ); 
    
    nrf_cmd( 0x20, 0x12 );    // TX on
    digitalWrite( cePin, HIGH ); // Enable Chip
    delay( 50 );        // 
    digitalWrite( cePin, LOW );   // (in preparation of switching to RX quickly)

    Serial.println();
}

uint8_t RF24Beacon::sendData( const void* _data, uint8_t _len ) {
    if( setData( _data, _len ) ) {
        beep();
        return 1;
    } else {
        return 0;
    }
}



void RF24Beacon::end( void ) {
    ch              = 0;
    advInterval     = 0;
    nameLen         = 0;
    dataLen         = 0;
    for( uint8_t i = 0; i < 32; i++ )
        buf[i] = 0x00;
    SPI.end();
}


// --------------------------------------------------------
// Private Functions 
uint8_t RF24Beacon::spi_byte( uint8_t byte ) {
    SPI.transfer( byte );
    // Serial.print( byte, HEX );  Serial.print(" ");
    return byte;
}

void RF24Beacon::nrf_cmd( uint8_t cmd, uint8_t data ) {
    digitalWrite( csnPin, LOW );
    spi_byte( cmd );
    spi_byte( data );
    digitalWrite( csnPin, HIGH );

}

void RF24Beacon::nrf_simplebyte( uint8_t byte ) {
    digitalWrite( csnPin, LOW );
    spi_byte( byte );
    digitalWrite( csnPin, HIGH );
}

void RF24Beacon::nrf_manybytes( uint8_t *data, uint8_t len ) {
    digitalWrite( csnPin, LOW );
    do {
        spi_byte( *data++ );
    }while( --len );
    digitalWrite( csnPin, HIGH );
}


// --------------------------------------------------------
// Utility functions

void btLeCrc( const uint8_t* data, uint8_t len, uint8_t* dst ) {
    uint8_t v, t, d;

    while( len-- ) {
        d = *data++;
        for( v = 0; v < 8; v++, d >>= 1 ) {
            t = dst[0] >> 7;
            dst[0] <<= 1;
            if( dst[1] & 0x80 ) {
                dst[0] |= 1;
            }
            dst[1] <<= 1;
            if( dst[2] & 0x80 ) {
                dst[1] |= 1;
            }
            dst[2] <<= 1;
            if( t != (d&1) ) {
                dst[2] ^= 0x5B;
                dst[1] ^= 0x06;
            }
        } 
    }
}

uint8_t swapbits( uint8_t a ) {

    uint8_t v = 0;
  
    if(a & 0x80) v |= 0x01;
    if(a & 0x40) v |= 0x02;
    if(a & 0x20) v |= 0x04;
    if(a & 0x10) v |= 0x08;
    if(a & 0x08) v |= 0x10;
    if(a & 0x04) v |= 0x20;
    if(a & 0x02) v |= 0x40;
    if(a & 0x01) v |= 0x80;

    return v;
}

void btLeWhiten( uint8_t* data, uint8_t len, uint8_t whitenCoeff ) {

    uint8_t  m;
  
    while( len-- ) {
        for( m=1; m; m<<=1 ) {
            if( whitenCoeff & 0x80 ) {
                whitenCoeff ^= 0x11;
                (*data) ^= m;
            }
            whitenCoeff <<= 1;
        }
        data++;
    }
}

uint8_t btLeWhitenStart( uint8_t chan ) {
    // the value we actually use is what BT'd use left 
    // shifted one...makes our life easier

    return swapbits(chan)|2;  
}

void btLePacketEncode( uint8_t* packet, uint8_t len, uint8_t chan ) {
    // length is of packet, including crc. pre-populate crc in packet 
    // with initial crc value!

    uint8_t i, dataLen = len - 3;
  
    btLeCrc(packet, dataLen, packet + dataLen);
    for( i=0; i<3; i++, dataLen++ ) {
        packet[dataLen] = swapbits(packet[dataLen]);
    }
    btLeWhiten( packet, len, btLeWhitenStart(chan) );
    for( i=0; i<len; i++ ) {
        packet[i] = swapbits(packet[i]);
    }
}