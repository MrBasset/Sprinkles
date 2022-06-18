#include <SD.h>
//#include <Ethernet.h>
#include <SPI.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Eventually.h>
#include <IniFile.h>

#include <math.h>
#include <String.h>

#define SD_SELECT 4
#define ETHERNET_SELECT 10

//Digital Pins 6-9 will be used for our water zones solenoids. We can overflow to Analog pins A0 to A3 for further
const uint8_t solenoidPins[8] = { 6, 7, 8, 9, A0, A1, A2, A3 };

const int MAX_WATER_ZONES = 8;

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
  int StartingDurationSeconds = 180;      //three minutes
  int MinimumThresholdTempCelcius = 21;   //the temperature at which the zone starts spraying
  int MaximumThresholdTempCelcius = 27;   //the temperature at which the zone will spray continuously for it's section of the duty cycle
} Configuration;

Configuration pConf;

// ---------- END DOE -----------

// ---------- Memory Debugging -------
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}

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
void printErrorMessage(uint8_t e, bool eol = true)
{
  switch (e) {
  case IniFile::errorNoError:
    Serial.print(F("no error"));
    break;
  case IniFile::errorFileNotFound:
    Serial.print(F("file not found"));
    break;
  case IniFile::errorFileNotOpen:
    Serial.print(F("file not open"));
    break;
  case IniFile::errorBufferTooSmall:
    Serial.print(F("buffer too small"));
    break;
  case IniFile::errorSeekError:
    Serial.print(F("seek error"));
    break;
  case IniFile::errorSectionNotFound:
    Serial.print(F("section not found"));
    break;
  case IniFile::errorKeyNotFound:
    Serial.print(F("key not found"));
    break;
  case IniFile::errorEndOfFile:
    Serial.print(F("end of file"));
    break;
  case IniFile::errorUnknownError:
    Serial.print(F("unknown error"));
    break;
  default:
    Serial.print(F("unknown error value"));
    break;
  }
  if (eol)
    Serial.println();
}


void ReadConfigurationFromSdCard()
{
  const size_t bufferLen = 80;
  char buffer[bufferLen];

  const char *filename = "/settings.ini";

  //initialise the SD card
  SPI.begin();
  if (!SD.begin(SD_SELECT)) {
    Serial.println(F("SD initialization failed!"));
    while (1);
  }

  IniFile ini(filename);
  if (!ini.open()) {
    Serial.print(F("Ini file "));
    Serial.print(filename);
    Serial.println(F(" does not exist"));
    // Cannot do anything else
    while (1);
  }
  Serial.println("Ini file exists");

  // Check the file is valid. This can be used to warn if any lines
  // are longer than the buffer.
  if (!ini.validate(buffer, bufferLen)) {
    Serial.print(F("ini file "));
    Serial.print(ini.getFilename());
    Serial.print(F(" not valid: "));
    printErrorMessage(ini.getError());
    // Cannot do anything else
    while (1);
  }
  
  Serial.println(F("Fetching Config Values"));

  // Fetch DutyCycleSeconds
  bool found = ini.getValue("Global", "DutyCycleSeconds", buffer, bufferLen, pConf.DutyCycleSeconds);
  if (found) {
    Serial.print(F("The value of 'DutyCycleSeconds' in section 'Global' is "));
    // Print value, converting boolean to a string
    Serial.println(pConf.DutyCycleSeconds);
  }
  else {
    Serial.print(F("Could not get the value of 'DutyCycleSeconds' in section 'Global': "));
    printErrorMessage(ini.getError());
  }
  
  // Fetch StartingDurationSeconds
  found = ini.getValue("Global", "StartingDurationSeconds", buffer, bufferLen, pConf.StartingDurationSeconds);
  if (found) {
    Serial.print(F("The value of 'StartingDurationSeconds' in section 'Global' is "));
    // Print value, converting boolean to a string
    Serial.println(pConf.StartingDurationSeconds);
  }
  else {
    Serial.print(F("Could not get the value of 'StartingDurationSeconds' in section 'Global': "));
    printErrorMessage(ini.getError());
  }

  // Fetch MinimumThresholdTempCelcius
  found = ini.getValue("Global", "MinimumThresholdTempCelcius", buffer, bufferLen, pConf.MinimumThresholdTempCelcius);
  if (found) {
    Serial.print(F("The value of 'MinimumThresholdTempCelcius' in section 'Global' is "));
    // Print value, converting boolean to a string
    Serial.println(pConf.MinimumThresholdTempCelcius);
  }
  else {
    Serial.print(F("Could not get the value of 'MinimumThresholdTempCelcius' in section 'Global': "));
    printErrorMessage(ini.getError());
  }

  // Fetch MaximumThresholdTempCelcius
  found = ini.getValue("Global", "MaximumThresholdTempCelcius", buffer, bufferLen, pConf.MaximumThresholdTempCelcius);
  if (found) {
    Serial.print(F("The value of 'MaximumThresholdTempCelcius' in section 'Global' is "));
    // Print value, converting boolean to a string
    Serial.println(pConf.MaximumThresholdTempCelcius);
  }
  else {
    Serial.print(F("Could not get the value of 'MaximumThresholdTempCelcius' in section 'Global': "));
    printErrorMessage(ini.getError());
  }
}

// ----------- END Configuration

// ------- BEGIN SETUP --------------

EvtManager mgr;

const int SENSOR_PIN = 2;				// defined the temperature sensor pin, all sensors will be connected to this.
OneWire oneWire(SENSOR_PIN);		// Create a 1-wire object

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

//Hold each of our water zones (max is 8)
WaterZone* pWaterZones;

int ActiveWaterZones = 0;

void setup() {

  // Configure all of the SPI select pins as outputs and make SPI
  // devices inactive, otherwise the earlier init routines may fail
  // for devices which have not yet been configured.
  pinMode(SD_SELECT, OUTPUT);
  digitalWrite(SD_SELECT, HIGH); // disable SD card
  
  pinMode(ETHERNET_SELECT, OUTPUT);
  digitalWrite(ETHERNET_SELECT, HIGH); // disable Ethernet

  //first lets make sure all our valves are closed
  for( int i = 0; i < 7; i++ ) {
    pinMode(solenoidPins[i], OUTPUT);
    CloseValve(solenoidPins[i]);
  }

  //wait for serial to attach
  while (!Serial);

  Serial.begin(9600);       // for diagnostics

  Serial.println("");
  Serial.println("");
  Serial.print(F("****** Starting Program *********"));
  Serial.println("");
  Serial.println("");

  //load the configuration from the file system (or create the default if it doesn't exist)
  ReadConfigurationFromSdCard();

  //Detect the temperature sensors
  discoverOneWireDevices();

  Serial.print(F("No. of ActiveWater Zones discovered: "));
  Serial.println(ActiveWaterZones);

  if(ActiveWaterZones == 0) {
    Serial.println(F("No active water zones found"));
    while(1);
  }

  Serial.println();
  Serial.print(F("Initializing Temperature Control Library Version "));
  Serial.println(DALLASTEMPLIBVERSION);

  // Initialize the Temperature measurement library
  sensors.begin();

  Serial.println(F("Iterating through the Water Zones to assign temperature probes"));
  for( int i = 0; i < ActiveWaterZones; i++) {
    sensors.setResolution(pWaterZones[i].Probe, 10);  // set the resolution to 10 bit (Can be 9 to 12 bits .. lower is faster)
  }
  
  Serial.println(F("initialising the event listeners & timers"));
  mgr.resetContext(); 


  Serial.print(F("Free Memory: "));
  Serial.println(freeMemory());

  Serial.println(F("Initialise temperator sensor poll for every 2 seconds."));
  mgr.addListener(new EvtTimeListener(2000, true, (EvtAction)PollTemperatureSensors));

  Serial.println(F("and give them time to stablise (approx 4000 * MAX_WATER_ZONES seconds) before starting the dutycycle"));
  mgr.addListener(new EvtTimeListener(4000 * MAX_WATER_ZONES, false, (EvtAction)SetupDutyCycleLoop));
}

bool PollTemperatureSensors()
{
  Serial.print(F("Getting temperatures... "));
  Serial.println();

  Serial.print(F("Free Memory: "));
  Serial.println(freeMemory());

  // Command all devices on bus to read temperature  
  sensors.requestTemperatures(); 

  for(int i = 0; i < ActiveWaterZones; i++)
  {
    Serial.print(F("Fetching temp for probe "));
    Serial.println(i);


  Serial.print(F("Free Memory: "));
  Serial.println(freeMemory());
    
    float tempC = sensors.getTempC(pWaterZones[i].Probe);
    if (tempC == -127.00) 
    {
      Serial.println(F("Error getting temperature  "));
      pWaterZones[i].IsProbeError = true;
    }
    else
    {
      Serial.print("C: ");
      Serial.println(tempC);

      pWaterZones[i].IsProbeError = false;
      pWaterZones[i].CurrentTemperature = tempC;
    }
  }

  
  Serial.print(F("Free Memory: "));
  Serial.println(freeMemory());

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
  Serial.print(F("Free Memory: "));
  Serial.println(freeMemory());
  
  Serial.println(F("Setting up duty cycle loop"));
  float zoneInterval = pConf.DutyCycleSeconds / ActiveWaterZones; //The amout of time (in seconds) each zone gets

  Serial.print(F("Zone interval is :"));
  Serial.println(zoneInterval);

  for (int i = 0; i < ActiveWaterZones; i++)
  {


    Serial.print(F("Free Memory: "));
    Serial.println(freeMemory());
    
    //establish when a zone starts its section of the Duty Cycle
    mgr.addListener(new EvtTimeListener(i * zoneInterval * 1000, true, (EvtAction)ProcessZone(i, zoneInterval)));
  }

  //continue the event chain with the new listeners we've added.
  return false;
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

    Serial.print(F("Switching zone "));
    Serial.print(zone);
    Serial.print(F(" on for "));
    Serial.print(openseconds);
    Serial.println(F(" seconds"));

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

void DebugProbeAddress (DeviceAddress address) {
  //some output debug, not strictly necessary
  for(int i = 0; i < 8; i++) {
    Serial.print(F("0x"));
    if (address[i] < 16) Serial.print('0');
    Serial.print(address[i], HEX);
    if (i < 7) Serial.print(F(", "));
  }
}

/*
 * Will automatically discover temperature sensors to a maximum of 8 sensors
 * the address is returned as an array of 8 bytes
 */
void discoverOneWireDevices() {

  DeviceAddress addresses[MAX_WATER_ZONES] = { 0 };
  int row = 0;
  
  Serial.print(F("Looking for 1-Wire devices...\n\r"));
  while(oneWire.search(addresses[row])) {
    Serial.print(F("\n\r\n\rFound \'1-Wire\' device with address:\n\r"));
    DebugProbeAddress(addresses[row]);

    ActiveWaterZones++;
    
    if (row == MAX_WATER_ZONES - 1) break;
    row++;
  }
  
  Serial.println();
  Serial.println(F("Done"));
  oneWire.reset_search();

  Serial.println(F("Setup WaterZones array with probe addresses"));
  //now initialise WaterZones as an array of the correct size (only done in setup to avoid heap fragmentation).
  if(pWaterZones != 0) delete [] pWaterZones;
  pWaterZones = new WaterZone[ActiveWaterZones];

  int crcFailure = 0;

  //populate WaterZones
  for( int i = 0; i < ActiveWaterZones; i++) {
    
    //CRC check
    if ( OneWire::crc8( addresses[i], 7) != addresses[i][7]) {
      Serial.print(F("CRC is not valid!\n\r"));
      crcFailure++;
      continue;
    }
    //The DeviceAddress is an unsigned array of 8-bits, have to use memcpy( arrB, arrA, 8 ); to copy it.
    memcpy( pWaterZones[i].Probe, addresses[i], 8 );

    Serial.println(F("Setting waterzone probe address as:"));
    DebugProbeAddress(pWaterZones[i].Probe);
    
    pWaterZones[i].Solenoid = solenoidPins[i];

    //set up pin
    pinMode(pWaterZones[i].Solenoid, OUTPUT);
    digitalWrite(pWaterZones[i].Solenoid, LOW); //ensure that it's closed
  }

  ActiveWaterZones = ActiveWaterZones - crcFailure;
  Serial.println(F("All zones configured"));
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
