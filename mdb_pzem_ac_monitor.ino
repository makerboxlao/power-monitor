#include <PZEM004Tv30.h>      // PZEM 004 v3.0 Library
#include <SoftwareSerial.h>   // Software TX/RX 
#include <ESP8266WiFi.h>      // WiFI library
#include <MySQL_Connection.h> // MySQL library
#include <MySQL_Cursor.h>     // MySQL query library
#include "config.h"           // Program configuration

//
// Verify basic configuration has been configured
// WIFI_SSID, WIFI_PASS, MYSQL_ADDR are required
//

#ifndef WIFI_SSID
#error Missing configuration (WIFI_SSID)
#endif

#ifndef WIFI_PASS
#error Missing configuration (WIFI_PASS)
#endif

#ifndef MYSQL_ADDR
#error Missing configuration (MYSQL_ADDR)
#endif

#ifndef MYSQL_PORT
// default MySQL port can be used if not assigned
#define MYSQL_PORT 3306
#endif

#ifndef MYSQL_USER
#define MYSQL_USER "root"
#endif

#ifndef MYSQL_PASS
#define MYSQL_PASS ""
#endif

// MAC Address for WiFi device
// https://miniwebtool.com/mac-address-generator/
// @TODO
// This may not be required and can be remove later
byte mac_addr[] = { 0x66, 0x44, 0x26, 0xAC, 0x61, 0x47 };

//
// MySQL Connection information
//

WiFiClient client;
MySQL_Connection conn( (Client *) &client );

//
// PZEM Connection information
//
// Pin D5 is used for (arduino) RX <- TX (pxem)
// Pin D2 is used for (arduino) TX -> RX (pzem)
//

SoftwareSerial pzemSWSerial( D5, D2 ); // RX, TX ports for pzem

int const PZEM_COUNT = 3;       // Number of PZEM units
int const PZEM_ADDR = 0x45;     // Starting PZEM address (0x45, 0x46, 0x47)
PZEM004Tv30 pzems[PZEM_COUNT];  // Initialize the PZEM

//
// Other configuration
//

int const delay_timer = 1000 * 60; // 1 minute between polling

// Setup
void setup()
{
  Serial.begin( 115200 );
  while( !Serial ); // wait for serial port to connect
  
  Serial.println( "MBL AC Power meter monitor" );

  // Begin WiFi
  WiFi.begin( WIFI_SSID, WIFI_PASS );
  
  // Connecting to WiFi...
  Serial.print( "Connecting to " );
  Serial.print( WIFI_SSID );
  
  // Loop continuously while WiFi is not connected
  while( WiFi.status() != WL_CONNECTED )
  {
    Serial.print( "." );
    delay( 500 ); // 0.5s delay between checking
  }

  // Connected to WiFi
  Serial.println();
  Serial.print( "Connected! IP address: ");
  Serial.println( WiFi.localIP() );

  // Update MAC Address of WiFi
  // @TODO
  // Check to see if this is required or not
  WiFi.macAddress( mac_addr );

  // Connect to MySQL Database Server
  checkDatabaseConnection();

  // Connect to the PZEM units
  for( int i = 0; i < PZEM_COUNT; i++ )
  {
    uint8_t addr = PZEM_ADDR + i;
    Serial.print( "Connecting to PZEM (0x" );
    Serial.print( addr, HEX );
    Serial.println( ")" );
    
    pzems[i] = PZEM004Tv30( pzemSWSerial, addr );

    Serial.print( "Connected PZEM ID: (0x" );
    Serial.print( pzems[i].readAddress(), HEX );
    Serial.println( ")" );
  }
  
  Serial.println();
}

typedef struct {
  int phase;
  float voltage;
  float current;
  float power;
  float energy;
  float frequency;
  float pf;
} pzemData;

void loop()
{
  int status = checkDatabaseConnection();
  
  for( int i = 0; i < PZEM_COUNT; i++ )
  {
    Serial.print( "PZEM: " );
    Serial.println( i );

    // Read the data from the sensor
    float voltage   = pzems[i].voltage();
    float current   = pzems[i].current();
    float power     = pzems[i].power();
    float energy    = pzems[i].energy();
    float frequency = pzems[i].frequency();
    float pf        = pzems[i].pf();
  
    // Check if the data is valid
    if( isnan( voltage ) )
      voltage = -1;
      
    if( isnan( current ) )
      current = -1;
      
    if( isnan( power ) )
      power = -1;
      
    if( isnan( energy ) )
      energy = -1;
      
    if( isnan( frequency ) )
      frequency = -1;
      
    if( isnan( pf ) )
      pf = -1;

    if( false == status )
    {
      // As we are not connected to the MySQL Database
      // we are going to store the information until it becomes available again
      // @TODO
      // Detect if we are about to run out of memory and flush out old data
      
    }
    else
    {
      // Prepare SQL statement to populate database
      char INSERT_SQL[255];
      sprintf( INSERT_SQL, "INSERT INTO mbl_power_usage.mdb_power(phase, voltage, current, power, energy, frequency, pf) VALUES (%d, %.02f, %.02f, %.02f, %.03f, %.01f, %.02f)", i + 1, voltage, current, power, energy, frequency, pf );
  
      // Execute the SQL statement
      MySQL_Cursor *cur_mem = new MySQL_Cursor( &conn );
      cur_mem->execute( INSERT_SQL );
      delete cur_mem;
      
      Serial.println( "Uploaded data Successfully." );
      Serial.println( "----------" );
      delay( 500 );
    }
  }

  // we only want to update once every minute
  Serial.println( "Waiting for next polling..." );
  delay( delay_timer );
}

// Function to check if still connected to database or not
// and attempt to reconnect
// @TODO
// Maybe change this to not retry indefinately, that way
// We can still poll the PZEM and store information into
// memory until the moment we can reconnect back to the
// Database server

int const connection_retries = 5;

bool checkDatabaseConnection()
{
  static int retry_count = 0;
  
  // Check if we are still connected
  if( !conn.connected() )
  {
    Serial.println( "Connecting to MySQL Database server" );
    if( conn.connect( MYSQL_ADDR, MYSQL_PORT, MYSQL_USER, MYSQL_PASS))
    {
      retry_count = 0;
      Serial.println( "Connection successful!" );
      return true;
    }
    else
    {
      retry_count++;
      if( retry_count > connection_retries )
      {
        Serial.println( "Connection failed, will try again next loop" );
        retry_count = 0;
        return false;
      }
      Serial.print( "Connection failed, trying again... (" );
      Serial.print( retry_count );
      Serial.print( " of " );
      Serial.print( connection_retries );
      Serial.println( ")" );
      delay( 100 );
      return checkDatabaseConnection();
    }
  }

  return true;
}