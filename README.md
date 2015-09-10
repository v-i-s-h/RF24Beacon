RF24Beacon
----------

Arduino library for using nRF24L01 modules as BLE beacons.


## Pin Connections
Arduino|nRF24L01+
-----------------
D13    | SCK
D12    | MISO
D11    | MOSI
D10    | CSN
D9     | CE
--     | IRQ
3V3    | Vcc
GND    | GND
-----------------

You can intialize RF24Beacon using
''' c++
RF24Beacon beacon( 9,10 );
'''
See examples folder for more information
