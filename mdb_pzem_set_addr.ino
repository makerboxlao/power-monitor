#include "includes.h"         // Main includes

//
// PZEM Connection information
//
// Pin D5 is used for (arduino) RX <- TX (pxem)
// Pin D2 is used for (arduino) TX -> RX (pzem)
//

SoftwareSerial pzemSWSerial( D5, D2 ); // RX, TX ports for pzem

// Connect to pzem using default broadcast address
PZEM004Tv30 pzem( pzemSWSerial );

//
// Set this to be any address ranging between 0x01 to 0xF8
// make sure you set this to something unique for each device
//
//
//
//

uint8_t addr = 0x45;

//
//
//
//
//

void setup()
{
  Serial.begin( 115200 );
  while( !Serial ); // wait for serial port to connect
  
  Serial.println( "MBL PZEM configation tool" );
}

void loop()
{
  Serial.println( "Checking to see if PZEM address matches the assigned address" );

  uint8_t cur_addr = pzem.readAddress();

  // Print out current custom address
  Serial.printf( "Current address:   0x%x\n", cur_addr );

  if( cur_addr == addr )
  {
    Serial.println( "Addresses matched! we can quit now." );
    exit( 0 );
  }

  Serial.println( "Address doesn't match against assigned address, will try and update it." );

  // Set the custom address
  Serial.printf( "Setting address to: 0x%x\n", addr );

  if( !pzem.setAddress( addr ) )
  {
    // Setting custom address failed.
    // Possible causes:
    // - no PZEM connected
    // - PZEM requires power being supplied from both ends of the pins as just the 5V from the esp2866 is not enough to flash the internal memory

    Serial.println( "Error setting address, will try again in 5 seconds..." );
  }
  else
  {
    // Print out the new custom address
    Serial.printf( "Updated address:    0x%x\n", pzem.readAddress() );
  }
  
  Serial.println();
  Serial.println( "----------------" );
  Serial.println();
  
  delay( 5000 );
}