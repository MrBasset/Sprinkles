#include <SD.h>
//#include <Ethernet.h>
#include <SPI.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Eventually.h>
#include <ArduinoJson.h>

#include <math.h>
#include <String.h>

#include <SoftwareReset.h>

/** Data Only Entities **/
typedef struct WaterZone
{
  float CurrentTemperature = 0;
  bool IsValveOpen = false;
  DeviceAddress Probe;
  bool IsProbeError = false;
  uint8_t Solenoid = 0;
} WaterZone;

typedef struct
{
  int DutyCycleSeconds = 30 * 60;         //30 minutes
  int NumberOfWaterZones = 4;             //four water zones
  int StartingDurationSeconds = 180;      //three minutes
  int MinimumThresholdTempCelcius = 21;   //the temperature at which the zone starts spraying
  int MaximumThresholdTempCelcius = 27;   //the temperature at which the zone will spray continuously for it's section of the duty cycle
} Configuration;

Configuration pConf;

// ---------- END DOE -----------

// ---------- RGB Status LED ---------

/**
 * Status:
 *  - Constant Red: Configuration/SD error
 *  - Flashing Red: Error Code
 *    - No temp sensors
 *    - No DHCP
 *    - Server Failure
 *  - Flashig Blue: Server Starting up
 *  - Constant Blue: Running
 *  - Flashing Gree: Web Server Acccess/Configuration Update
 */

// ---------- END RGB Status LED -----

//----------- BEGIN Configuration --------------------
void ReadConfigurationFromSdCard()
{
  const String FILE_PATH = "/mnt/sd/arduino/settings.json";
  const byte bufferSize = 150; //define number of bytes expected on the JSON thread
  char json[bufferSize]; //create a char array buffer that is 150 bytes in size in this case

  File dataFile = SD.open(FILE_PATH, FILE_READ);
  dataFile.readBytes(json, bufferSize);
  dataFile.close();

  StaticJsonBuffer<JSON_OBJECT_SIZE(4)> jsonBuffer;  //We know our JSON string just has 4 objects only and no arrays, so our buffer is set to handle that many
  JsonObject& root = jsonBuffer.parseObject(json);    // breaks the JSON string into individual items, saves into JsonObject 'root'

  if (!root.success()) {
    Serial.println(F("parseObject() failed"));
    return;   //if parsing failed, this will exit the function and not change the values to 00
  }
  else 
  {
    // update variables
    pConf.DutyCycleSeconds = root["DutyCycleSeconds"];
    pConf.NumberOfWaterZones = root["NumberOfWaterZones"];
    pConf.StartingDurationSeconds = root["StartingDurationSeconds"];
    pConf.MinimumThresholdTempCelcius = root["MinimumThresholdTempCelcius"];
    pConf.MaximumThresholdTempCelcius = root["MaximumThresholdTempCelcius"];
  }
}

void WriteConfigurationToSdCard(Configuration configuration)
{
  softwareReset(STANDARD);
  
	return;
}

// ----------- END Configuration

// ----------- BEGIN JSON REST API -------------------

/**
 * Accepts an inbound HTTP request and translates this into the
 * relevent operations.
 **/
//void HandleRequest(EthernetClient& client) {
//  
//  bool currentLineIsBlank = true;
//  
//  while (client.connected()) {
//    if (client.available()) {
//      char c = client.read();
//      if (c == '\n' && currentLineIsBlank) {
//        return true;
//      } else if (c == '\n') {
//        currentLineIsBlank = true;
//      } else if (c != '\r') {
//        currentLineIsBlank = false;
//      }
//    }
//  }
//  return false;
//}

//void ReturnError(EthernetClient& client, JsonObject& json) {
//  client.println("HTTP/1.1 400 Bad Request");
//  client.println("Content-Type: application/json");
//  client.println("Connection: close");
//  client.println();
//  client.println("{ \"error\" : \"Unknown Function\" }");
//}
//
//void writeResponse(EthernetClient& client, JsonObject& json) {
//  client.println("HTTP/1.1 200 OK");
//  client.println("Content-Type: application/json");
//  client.println("Connection: close");
//  client.println();
//
//  json.prettyPrintTo(client);
//}

//NOTE: Example JSON Response
JsonObject& prepareResponse(JsonBuffer& jsonBuffer) {
  JsonObject& root = jsonBuffer.createObject();

  JsonArray& analogValues = root.createNestedArray("analog");
  for (int pin = 0; pin < 6; pin++) {
    int value = analogRead(pin);
    analogValues.add(value);
  }

  JsonArray& digitalValues = root.createNestedArray("digital");
  for (int pin = 0; pin < 14; pin++) {
    int value = digitalRead(pin);
    digitalValues.add(value);
  }

  return root;
}

void GetConfiguration() {}
void SetConfiguration(String request) {}

void GetAllZoneSummaries(){}
void GetZoneSummary(String request){}

void OverrideCloseValve(int valve){}
void OverrideOpenValve(int valve){}
void GetValveStatus(int valve){}


//------ BEGIN Event management


void HandleTemperatureThreasholdExceeded(){}

void HandleSprayTimerStarted(){}
void HandleSprayTimerExpired(){}

void HandleConfigurationChange(){}

// ------- BEGIN SETUP --------------

//byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};  //TODO: set this to the MAC on the Ethernet sheild
//EthernetServer Server(80);

EvtManager mgr;

const int SENSOR_PIN = 2;				// defined the temperature sensor pin, all sensors will be connected to this.
OneWire oneWire(SENSOR_PIN);		// Create a 1-wire object

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

//Hold each of our water zones (max is 8)
WaterZone* pWaterZones;

int ActiveWaterZones = 0;

void setup() {

  Serial.begin(9600);       // for diagnostics

  //load the configuration from the file system (or create the default if it doesn't exist)

  //Detect the temperature sensors
  discoverOneWireDevices();

  Serial.println(
  Serial.print(F("Initializing Temperature Control Library Version "));
  Serial.println(DALLASTEMPLIBVERSION);

  // Initialize the Temperature measurement library
  sensors.begin();

  Serial.println(F("Iterating through the Water Zones to assign temperature probes"));
  for( int i = 0; i < ActiveWaterZones; i++) {
    sensors.setResolution(pWaterZones[i].Probe, 10);  // set the resolution to 10 bit (Can be 9 to 12 bits .. lower is faster)
  }
  
  //start up the web server and set to listen on port 80
  //if (Ethernet.begin(mac) == 0)
  //{
    //Serial.println("Failed to configure ethernet using DHCP");
    //TODO: DHCP error code
  //}

  //Server.begin(); //start web server
  
  Serial.println(F("initialising the event listeners & timers"));
  mgr.resetContext(); 
  
  //use a timer to poll the web server for requests every 500 milliseconds
  //mgr.addListener(new EvtTimeListener(500, true, (EvtAction)PollServer));

  Serial.println(F("Initialise temperator sensor poll for every 2 seconds."));
  mgr.addListener(new EvtTimeListener(2000, true, (EvtAction)PollTemperatureSensors));

  Serial.println(F("and give them time to stablise (approx 5 seconds) before starting the dutycycle"));
  mgr.addListener(new EvtTimeListener(5000, false, (EvtAction)SetupDutyCycleLoop()));
}

bool PollTemperatureSensors()
{
  Serial.print("Getting temperatures... ");  
  Serial.println();

  // Command all devices on bus to read temperature  
  sensors.requestTemperatures(); 

  for(int i = 0; i < ActiveWaterZones; i++)
  {
    float tempC = sensors.getTempC(pWaterZones[i].Probe);
    if (tempC == -127.00) 
    {
      Serial.print("Error getting temperature  ");
      pWaterZones[i].IsProbeError = true;
    }
    else
    {
      Serial.print("C: ");
      Serial.print(tempC);

      pWaterZones[i].IsProbeError = false;
      pWaterZones[i].CurrentTemperature = tempC;
    }
  }

  //stop the current action chain
  return false;
}

/*
 * use timers to set up the "start" interval across the duty cycle for each zone, e.g.
 * if the duty cycle is 30 mins and we have four zones, the "open" periods for each zone
 * are, 0, 7.5, 15 and 22.5 minutes. We set up a timer to check at the start of each interval
 * what the temperature is. This will then call the open valve if the threshold is exceeded.
 * 
 * A seperate timer will then be configured to "kill" the valve depending on the amount
 * that the temperature is exceeded.
 */
bool SetupDutyCycleLoop()
{
  float zoneInterval = pConf.DutyCycleSeconds / ActiveWaterZones; //The amout of time (in seconds) each zone gets

  for (int i = 0; i < ActiveWaterZones; i++)
  {
    //establish when a zone starts its section of the Duty Cycle
    mgr.addListener(new EvtTimeListener(i * zoneInterval, true, (EvtAction)ProcessZone(i, zoneInterval)));
  }

  //continue the event chain with the new listeners we've added.
  return true;
}

bool ProcessZone(int zone, float maximumInterval)
{
  if(pWaterZones[zone].CurrentTemperature > pConf.MinimumThresholdTempCelcius)
  {
    //valve needs to be on, lets work out for how long
    float openseconds = 0;

    if (pWaterZones[zone].CurrentTemperature > pConf.MaximumThresholdTempCelcius)
    {
      openseconds = maximumInterval;
    }
    else 
    {
      float difference = pConf.MaximumThresholdTempCelcius - pConf.MinimumThresholdTempCelcius;
      float interval = maximumInterval / difference;
      openseconds = (pWaterZones[zone].CurrentTemperature - pConf.MinimumThresholdTempCelcius + 1) * interval;

      if (openseconds > maximumInterval) openseconds = maximumInterval;
    }

    Serial.print("Switching zone ");
    Serial.print(zone);
    Serial.print(" on for ");
    Serial.print(openseconds);
    Serial.println(" seconds");

    //Open the valve
    OpenValve(pWaterZones[zone].Solenoid);
    //set up valve close
    mgr.addListener(new EvtTimeListener(openseconds, false, (EvtAction)CloseValve(pWaterZones[zone].Solenoid)));

    //work the new event
    return true;
  }

  //no further action
  return false;
}

bool PollServer()
{
//  EthernetClient client = Server.available();
//  if (client) {
//    HandleRequest(client);
//  }

  //all done here
  return false;
}

/*
 * Will automatically discover temperature sensors to a maximum of 8 sensors
 * the address is returned as an array of 8 bytes
 */
void discoverOneWireDevices() {

  DeviceAddress addresses[8] = { 0 };
  int row = 0;
  
  Serial.print("Looking for 1-Wire devices...\n\r");
  while(oneWire.search(addresses[row])) {
    Serial.print("\n\r\n\rFound \'1-Wire\' device with address:\n\r");

    //some output debug, not strictly necessary
    for(int i = 0; i < 8; i++) {
      Serial.print("0x");
      if (addresses[row][i] < 16) Serial.print('0');
      Serial.print(addresses[row][i], HEX);
      if (i < 7) Serial.print(", ");
    }

    ActiveWaterZones++;

    if (row == 7) break;
    row++;
  }
  
  Serial.println();
  Serial.print("Done");
  oneWire.reset_search();

  //now initialise WaterZones as an array of the correct size (only done in setup to avoid heap fragmentation).
  if(pWaterZones != 0) delete [] pWaterZones;
  pWaterZones = new WaterZone[ActiveWaterZones];

  int crcFailure = 0;

  //Digital Pins 6-9 will be used for our water zones solenoids. We can overflow to Analog pins A0 to A3 for further
  uint8_t solenoidPins[8] = { 9, 8, 7, 6, A0, A1, A2, A3 };

  //populate WaterZones
  for( int i = 0; i < ActiveWaterZones; i++) {
    
    //CRC check
    if ( OneWire::crc8( addresses[i], 7) != addresses[i][7]) {
      Serial.print("CRC is not valid!\n\r");
      crcFailure++;
      continue;
    }
    //The DeviceAddress is an unsigned array of 8-bits, have to use memcpy( arrB, arrA, 5 ); to copy it.
    memcpy( pWaterZones[i].Probe, addresses[i], 8 );
    pWaterZones[i].Solenoid = solenoidPins[i];

    //set up pin
    pinMode(pWaterZones[i].Solenoid, OUTPUT);
    digitalWrite(pWaterZones[i].Solenoid, LOW); //ensure that it's closed
  }

  ActiveWaterZones = ActiveWaterZones - crcFailure;
}

// ---------- END SETUP --------------

USE_EVENTUALLY_LOOP(mgr)

bool OpenValve(int pin) 
{
  digitalWrite(pin, HIGH);

  return false;
}
bool CloseValve(int pin) 
{
  digitalWrite(pin, LOW);

  return false;
}

// ---------- END PIN FUNCTIONS ---
