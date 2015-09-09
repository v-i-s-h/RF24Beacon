// RF24 Beacon

#include <SPI.h>

#include "RF24Beacon.h"

RF24Beacon beacon( 9,10 );

uint8_t customData[] = { 0x01, 0x02, 0x03 };
    
void setup() {

    beacon.begin();
    beacon.setMAC( 0x01, 0x02, 0x03, 0x04, 0x05, 0xF6 );
    
    uint8_t temp = beacon.setName( "myBeacon" );
    beacon.setData( customData, 2 );

}

void loop() {

    // beacon.sendData( customData, sizeof(customData) );
    beacon.beep();
    
    delay( 1000 );
}