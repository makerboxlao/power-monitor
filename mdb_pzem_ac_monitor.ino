#include "config.h"           // Program configuration
#include "includes.h"         // Main includes

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

//
// MySQL Connection information
//

WiFiClient client;
MySQL_Connection conn( (Client *) &client );

//
// PZEM Connection information
//
// Pin D2 is used for (arduino) RX <- TX (pxem)
// Pin D5 is used for (arduino) TX -> RX (pzem)
//

SoftwareSerial pzemSWSerial( D2, D5 ); // RX, TX ports for pzem

int const PZEM_COUNT = 3;       // Number of PZEM units
int const PZEM_ADDR = 0x45;     // Starting PZEM address
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
  Serial.printf( "Connecting to %s\n", WIFI_SSID );
  
  // Loop continuously while WiFi is not connected
  while( WiFi.status() != WL_CONNECTED )
  {
    Serial.print( "." );
    delay( 500 ); // 0.5s delay between checking
  }

  // Connected to WiFi
  Serial.println();
  Serial.printf( "Connected! IP address: %s\n", WiFi.localIP().toString().c_str() );

  DateTime.setServer( "ntp.laodc.com" );
  DateTime.setTimeZone( "UTC" );
  DateTime.begin();

  if( !DateTime.isTimeValid() )
    Serial.println( "Failed to get time from server.");
  else
    Serial.printf( "Date Now is %s\n", DateTime.toISOString().c_str() );

  // Connect to MySQL Database Server
  checkDatabaseConnection();

  // Connect to the PZEM units
  for( int i = 0; i < PZEM_COUNT; i++ )
  {
    uint8_t addr = PZEM_ADDR + i;
    Serial.printf( "Connecting to PZEM (0x%x)\n", addr );
    
    pzems[i] = PZEM004Tv30( pzemSWSerial, addr );

    Serial.printf( "Connected PZEM ID: (0x%x)\n", pzems[i].readAddress() );
  }
  
  Serial.println();
}

StackArray<pzemStruct*> pzemData;

void loop()
{
  int status = checkDatabaseConnection();

  // check to see if we have pending power usage in the queue
  if( true == status && pzemData.count() > 0 )
  {
    Serial.println( "Backlog of data detected. attempting to backfill data to Database..." );
    while( !pzemData.isEmpty() )
    {
      pzemStruct *record = pzemData.pop();
      addRecord( record );
      Serial.printf( "Backlog dated %s added successfully.\n", record->timestamp.toISOString().c_str() );
      delay( 25 );
    }
  }
  
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

    pzemStruct *data = new pzemStruct();
    data->phase = i + 1;
    data->voltage = voltage;
    data->current = current;
    data->power = power;
    data->energy = energy;
    data->frequency = frequency;
    data->pf = pf;
    data->timestamp = DateTime;

    if( false == status )
    {
      // As we are not connected to the MySQL Database
      // we are going to store the information until it becomes available again
      pzemData.unshift( data );

      Serial.println( "Storing data into memoty while MySQL Database connection is down." );
      Serial.println( "----------" );
    }
    else
    {
      data->timestamp = 0;
      addRecord( data );
      delete data;
      
      Serial.println( "Uploaded data Successfully." );
      Serial.println( "----------" );
      delay( 500 );
    }
  }

  // we only want to update once every minute
  Serial.println( "Waiting for next polling..." );
  delay( delay_timer );
}

bool addRecord( pzemStruct *record )
{
  // Prepare SQL statement to populate database
  char INSERT_SQL[255];

  // Check to see if a timestamp was set (backlog)
  // if so, prepare the SQL statement to include it
  char timestamp[255];
  sprintf( timestamp, "" );
  if( record->timestamp != 0 )
    sprintf( timestamp, "%s%s%s", ", \"", record->timestamp.toISOString().c_str(), "\"" );

  sprintf(
    INSERT_SQL,
    "INSERT INTO mbl_power_usage.mdb_power"
    "( phase, voltage, current, power, energy, frequency, pf%s ) "
    "VALUES "
    "( %d, %f, %f, %f, %f, %f, %f%s )",
    ( record->timestamp != 0 ) ? ", timestamp" : "",
    record->phase,
    record->voltage,
    record->current,
    record->power,
    record->energy,
    record->frequency,
    record->pf,
    timestamp
  );

  // Execute the SQL statement
  MySQL_Cursor *cur_mem = new MySQL_Cursor( &conn );
  cur_mem->execute( INSERT_SQL );
  delete cur_mem;
  return true;
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
      Serial.println( "Connection failed, trying again..." );
      delay( 100 );
      return checkDatabaseConnection();
    }
  }

  return true;
}