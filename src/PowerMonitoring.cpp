/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "/Users/abdulhannanmustajab/Desktop/IoT/Power-Monitoring/PowerMonitoring/src/PowerMonitoring.ino"
/*
 * Project PowerMonitoring
 * Description:
 * Author:
 * Date:
 */

// v1.00  - First release: Changed to emonLibrary, state machine working fine on argon, 10 seconds publish frequency. 
// v1.01  - Fixed error state bug, added waituntill particle connect, added functions for changing constant value and showing them in console.
// v1.02  - Added check in takeMeasurements to send data if sensor is connected. 
// v1.03 -  Fixed pin chart.
// v1.04 - Added a particle function to enable sensors from the console.

void setup();
void loop();
void loadSystemDefaults();
void checkSystemValues();
void watchdogISR();
void petWatchdog();
void keepAliveMessage();
void sendEvent();
void UbidotsHandler(const char *event, const char *data);
void blinkLED(int LED);
int measureNow(String command);
int setVerboseMode(String command);
void publishStateTransition(void);
int setThirdPartySim(String command);
int setKeepAlive(String command);
void getBatteryContext();
int setConstantOne(String command);
int setConstantTwo(String command);
int setConstantThree(String command);
int setConstantFour(String command);
int setConstantFive(String command);
int setConstantSix(String command);
void updateConstantValues();
int enableSensor(String Sensor);
bool takeMeasurements();
void getBatteryCharge();
#line 14 "/Users/abdulhannanmustajab/Desktop/IoT/Power-Monitoring/PowerMonitoring/src/PowerMonitoring.ino"
const char releaseNumber[8] = "1.04";                                                      // Displays the release on the menu

// Included Libraries
#include "math.h"
#include "PublishQueueAsyncRK.h"                                                            // Async Particle Publish
#include "MB85RC256V-FRAM-RK.h"                                                             // Rickkas Particle based FRAM Library
#include "MCP79410RK.h"                                                                     // Real Time Clock
#include "EmonLib.h"                                                                        // Include Emon Library


// Prototypes and System Mode calls
SYSTEM_MODE(AUTOMATIC);                                                                     // This will enable user code to start executing automatically.
SYSTEM_THREAD(ENABLED);                                                                     // Means my code will not be held up by Particle processes.
STARTUP(System.enableFeature(FEATURE_RESET_INFO));
MB85RC64 fram(Wire, 0);                                                                     // Rickkas' FRAM library
MCP79410 rtc;                                                                               // Rickkas MCP79410 libarary
retained uint8_t publishQueueRetainedBuffer[2048];                                          // Create a buffer in FRAM for cached publishes
PublishQueueAsync publishQueue(publishQueueRetainedBuffer, sizeof(publishQueueRetainedBuffer));
Timer keepAliveTimer(1000, keepAliveMessage);

// Define the memory map - note can be EEPROM or FRAM - moving to FRAM for speed and to avoid memory wear
namespace FRAM {                                                                            // Moved to namespace instead of #define to limit scope
  enum Addresses {
    versionAddr           = 0x00,                                                           // Where we store the memory map version number - 8 Bits
    sysStatusAddr         = 0x01,                                                           // This is the status of the device
    alertStatusAddr       = 0x50,                                                           // Where we store the status of the alerts in the system
    sensorDataAddr        = 0xA0,                                                           // Where we store the latest sensor data readings
    sensorConstantsAddr   = 0xF0                                                            // Where we store CT sensor constant values.
   };
};

const int FRAMversionNumber = 9;                                                            // Increment this number each time the memory map is changed

struct systemStatus_structure {                     
  uint8_t structuresVersion;                                                                // Version of the data structures (system and data)
  bool thirdPartySim;                                                                       // If this is set to "true" then the keep alive code will be executed
  int keepAlive;                                                                            // Keep alive value for use with 3rd part SIMs
  uint8_t connectedStatus;
  uint8_t verboseMode;
  uint8_t lowBatteryMode;
  int stateOfCharge;                                                                        // Battery charge level
  uint8_t batteryState;                                                                     // Stores the current battery state
  int resetCount;                                                                           // reset count of device (0-256)
  unsigned long lastHookResponse;                                                           // Last time we got a valid Webhook response
  bool sensorOneConnected = true;                                                                  // Check if sensor One is connected.                                   
  bool sensorTwoConnected = false;                                                                  // Check if sensor Two is connected.                                   
  bool sensorThreeConnected = false;                                                                // Check if sensor Three is connected.                                   
  bool sensorFourConnected = false;                                                                 // Check if sensor Three is connected.                                   
  bool sensorFiveConnected = false;                                                                 // Check if sensor Three is connected.                                   
  bool sensorSixConnected = false;                                                                  // Check if sensor Three is connected.                                   
  uint8_t operatingMode;                                                                    // Check the operation mode,  
  /*
    * 1 if single phase mode
    * 2 if three phase mode with 3 wires.
    * 3 if three phase mode with 4 wires.
  */                   
  int Vrms;          // Default voltage value     
  
} sysStatus;

struct sensor_constants{
   double sensorOneConstant = 90.91;
   double sensorTwoConstant = 90.91;
   double sensorThreeConstant = 90.91;
   double sensorFourConstant = 90.91;
   double sensorFiveConstant = 90.91;
   double sensorSixConstant = 90.91;
} sensorConstants;

struct sensor_data_struct {                                                               // Here we define the structure for collecting and storing data from the sensors
  float sensorOneCurrent=0;
  float sensorTwoCurrent=0;
  float sensorThreeCurrent=0;
  float sensorFourCurrent=0;
  float sensorFiveCurrent=0;
  float sensorSixCurrent=0;
  unsigned long timeStamp;
  int stateOfCharge;
  bool validData;
} sensorData;


// State Machine Variables
enum State { INITIALIZATION_STATE, ERROR_STATE, IDLE_STATE, MEASURING_STATE, REPORTING_STATE, RESP_WAIT_STATE};
char stateNames[8][26] = {"Initialize", "Error", "Idle", "Measuring","Reporting", "Response Wait"};
State state = INITIALIZATION_STATE;
State oldState = INITIALIZATION_STATE;


// Pin Constants
const int blueLED =   D7;                                                               // This LED is on the Electron itself
const int wakeUpPin = D8;  
const int donePin = D5;

volatile bool watchdogFlag=false;                                                           // Flag to let us know we need to pet the dog

// Timing Variables
const unsigned long webhookWait = 45000;                                                    // How long will we wair for a WebHook response
const unsigned long resetWait   = 300000;                                                   // How long will we wait in ERROR_STATE until reset

unsigned long webhookTimeStamp  = 0;                                                        // Webhooks...
unsigned long resetTimeStamp    = 0;                                                        // Resets - this keeps you from falling into a reset loop
bool dataInFlight = false;

// Variables Related To Particle Mobile Application Reporting
// Simplifies reading values in the Particle Mobile Application
char temperatureString[16];
char humidityString[16];
char batteryContextStr[16];                                                                 // One word that describes whether the device is getting power, charging, discharging or too cold to charge

// Constant Values to be printed in console.
char sensorOneConstantStr[16];                                                                      // String for Sensor One Constant Value
char sensorTwoConstantStr[16];
char sensorThreeConstantStr[16];
char sensorFourConstantStr[16];
char sensorFiveConstantStr[16];
char sensorSixConstantStr[16];
char batteryString[16];

bool sysStatusWriteNeeded = false;                                                       // Keep track of when we need to write     
bool sensorDataWriteNeeded = false; 
bool constantsStatusWriteNeeded = false;
float voltage;                                                                             // Battery voltge Argon
// Variables
double current_irms             = 0;
double previous_irms            = 0;

// Time Period Related Variables
const int wakeBoundary = 0*3600 + 0*60 + 10;                                                // 0 hour 20 minutes 0 seconds

/************************CT LIBRARY RELATED Instances, Pinouts etc.***********************************/

//  Pins assignment/Functions definition-> DON'T CHANGE/MODIFY THESE                               
uint8_t CT1_PIN=A5;
uint8_t CT2_PIN=A4;
uint8_t CT3_PIN=A3;
uint8_t CT4_PIN=A2;
uint8_t CT5_PIN=A1;
uint8_t CT6_PIN=A0;

// Initialize the emon library.

EnergyMonitor emon1,emon2,emon3,emon4,emon5,emon6;               // Create an instance


/*************************************END************************************************************/


// setup() runs once, when the device is first turned on.
void setup() {
  pinMode(wakeUpPin,INPUT);                                                                 // This pin is active HIGH, 
  pinMode(donePin,OUTPUT);                                                                  // Allows us to pet the watchdog

  petWatchdog();                                                                            // Pet the watchdog - This will reset the watchdog time period AND 
  attachInterrupt(wakeUpPin, watchdogISR, RISING);                                          // The watchdog timer will signal us and we have to respond


  char StartupMessage[64] = "Startup Successful";                                           // Messages from Initialization
  state = INITIALIZATION_STATE;

  Particle.subscribe(System.deviceID() + "/hook-response/powermonitoring_hook/", UbidotsHandler, MY_DEVICES);

  // Particle Variables for debugging and mobile application
  Particle.variable("Release",releaseNumber);
  Particle.variable("Battery", batteryString);                                              // Battery level in V as the Argon does not have a fuel cell
  Particle.variable("BatteryContext",batteryContextStr);
  Particle.variable("Keep Alive Sec",sysStatus.keepAlive);
  Particle.variable("3rd Party Sim", sysStatus.thirdPartySim);
  
  Particle.variable("Constant One", sensorConstants.sensorOneConstant);
  Particle.variable("Constant Two", sensorConstants.sensorTwoConstant);
  Particle.variable("Constant Three", sensorConstants.sensorThreeConstant);
  Particle.variable("Constant Four", sensorConstants.sensorFourConstant);
  Particle.variable("Constant Five", sensorConstants.sensorFiveConstant);
  Particle.variable("Constant Six", sensorConstants.sensorSixConstant);


  Particle.function("Measure-Now",measureNow);
  Particle.function("Verbose-Mode",setVerboseMode);
  Particle.function("Keep Alive",setKeepAlive);
  Particle.function("3rd Party Sim", setThirdPartySim);
  Particle.function("Set Constant One",setConstantOne);
  Particle.function("Set Constant Two",setConstantTwo);
  Particle.function("Set Constant Three",setConstantThree);
  Particle.function("Set Constant Four",setConstantFour);
  Particle.function("Set Constant Five",setConstantFive);
  Particle.function("Set Constant Six",setConstantSix);
  Particle.function("Enable Sensor",enableSensor);                                          // Use this function to enable or disable a sensor from the console.


  rtc.setup();                                                        // Start the real time clock
  rtc.clearAlarm();                                                   // Ensures alarm is still not set from last cycle

  // Load FRAM and reset variables to their correct values
  fram.begin();                                                                             // Initialize the FRAM module

  byte tempVersion;
  fram.get(FRAM::versionAddr, tempVersion);
  if (tempVersion != FRAMversionNumber) {                                                   // Check to see if the memory map in the sketch matches the data on the chip
    fram.erase();                                                                           // Reset the FRAM to correct the issue
    fram.put(FRAM::versionAddr, FRAMversionNumber);                                         // Put the right value in
    fram.get(FRAM::versionAddr, tempVersion);                                               // See if this worked
    if (tempVersion != FRAMversionNumber) state = ERROR_STATE;                              // Device will not work without FRAM
    else {
      loadSystemDefaults();                                                                 // Out of the box, we need the device to be awake and connected
    }
  }
  else {
    fram.get(FRAM::sysStatusAddr,sysStatus);                                                // Loads the System Status array from FRAM
  }

  checkSystemValues();                                                                      // Make sure System values are all in valid range

  // Initialize all emon instances and sensors.
  emon1.current(CT1_PIN,sensorConstants.sensorOneConstant);
  emon2.current(CT2_PIN,sensorConstants.sensorTwoConstant);
  emon3.current(CT3_PIN,sensorConstants.sensorThreeConstant);
  emon4.current(CT4_PIN,sensorConstants.sensorFourConstant);
  emon5.current(CT5_PIN,sensorConstants.sensorFiveConstant);
  emon6.current(CT6_PIN,sensorConstants.sensorSixConstant);

  if (sysStatus.thirdPartySim) {
    waitFor(Particle.connected,30 * 1000); 
    Particle.keepAlive(sysStatus.keepAlive);                                              // Set the keep alive value
    keepAliveTimer.changePeriod(sysStatus.keepAlive*1000);                                  // Will start the repeating timer
  }

  takeMeasurements();                                                                       // For the benefit of monitoring the device

  if(sysStatus.verboseMode) publishQueue.publish("Startup",StartupMessage,PRIVATE);                       // Let Particle know how the startup process went

  waitUntil(Particle.connected);

  if (state == INITIALIZATION_STATE) state = IDLE_STATE;                                    // We made it throughgo let's go to idle
  
  publishQueue.publish("DEBUG- Startup",stateNames[state],PRIVATE);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  switch(state) {
  case IDLE_STATE:                                                                          // Idle state - brackets only needed if a variable is defined in a state    
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();

    if (!(Time.now() % wakeBoundary)) state = MEASURING_STATE;                                                     
    
    break;

  case MEASURING_STATE:                                                                     // Take measurements prior to sending
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();
    if (!takeMeasurements())
    {
      state = IDLE_STATE;
    }
    else {
      state = REPORTING_STATE;
      // previous_irms = current_irms;
    }
    break;

  case REPORTING_STATE: 
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();               // Reporting - hourly or on command
    if (Particle.connected()) {
      if (Time.hour() == 12) Particle.syncTime();                                           // Set the clock each day at noon
      takeMeasurements();
      sendEvent();                                                                          // Send data to Ubidots
      state = RESP_WAIT_STATE;                                                              // Wait for Response
    }
    else {
      publishQueue.publish("DEBUG- Startup","ERROR FROM REPORTING",PRIVATE);
      publishQueue.publish("DEBUG- PARTICLE CONNECTED",String(Particle.connected()),PRIVATE);
      state = ERROR_STATE;
      resetTimeStamp = millis();
    }
    break;

  case RESP_WAIT_STATE:
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();

    if (!dataInFlight && (Time.now() % wakeBoundary))                                       // Response received back to IDLE state - make sure we don't allow repetivie reporting events
    {
     state = IDLE_STATE;
    }
    else if (millis() - webhookTimeStamp > webhookWait) {                                   // If it takes too long - will need to reset
      resetTimeStamp = millis();
      publishQueue.publish("spark/device/session/end", "", PRIVATE);                        // If the device times out on the Webhook response, it will ensure a new session is started on next connect
      state = ERROR_STATE;                                                                  // Response timed out
      resetTimeStamp = millis();
    }
    break;

  
  case ERROR_STATE:                                                                         // To be enhanced - where we deal with errors
    if (state != oldState) publishStateTransition();
    if (millis() > resetTimeStamp + resetWait)
    {
      if (Particle.connected()) publishQueue.publish("State","Error State - Reset", PRIVATE); // Brodcast Reset Action
      delay(2000);
      System.reset();
    }
    break;
  }

  rtc.loop();                                                                               // keeps the clock up to date

  if (watchdogFlag) petWatchdog();                                                          // Watchdog flag is raised - time to pet the watchdog

  if (sysStatusWriteNeeded) {
    fram.put(FRAM::sysStatusAddr,sysStatus);
    sysStatusWriteNeeded = false;
  }

  if (sensorDataWriteNeeded) {
    fram.put(FRAM::sensorDataAddr,sensorData);
    sensorDataWriteNeeded = false;
  }

  if (constantsStatusWriteNeeded) {
    fram.put(FRAM::sensorConstantsAddr,sensorConstants);
    constantsStatusWriteNeeded = false;
  }

}


void loadSystemDefaults() {                                                                 // Default settings for the device - connected, not-low power and always on
  if (Particle.connected()) publishQueue.publish("Mode","Loading System Defaults", PRIVATE);
  sysStatus.thirdPartySim = 0;
  sysStatus.keepAlive = 120;
  sysStatus.structuresVersion = 1;
  sysStatus.verboseMode = false;
  sysStatus.lowBatteryMode = false;
  fram.put(FRAM::sysStatusAddr,sysStatus);                                                  // Write it now since this is a big deal and I don't want values over written
}


void checkSystemValues() {                                                                  // Checks to ensure that all system values are in reasonable range 
  if (sysStatus.connectedStatus < 0 || sysStatus.connectedStatus > 1) {
    if (Particle.connected()) sysStatus.connectedStatus = true;
    else sysStatus.connectedStatus = false;
  }
  if (sysStatus.keepAlive < 0 || sysStatus.keepAlive > 1200) sysStatus.keepAlive = 600;
  if (sysStatus.verboseMode < 0 || sysStatus.verboseMode > 1) sysStatus.verboseMode = true;
  if (sysStatus.lowBatteryMode < 0 || sysStatus.lowBatteryMode > 1) sysStatus.lowBatteryMode = 0;
  if (sysStatus.resetCount < 0 || sysStatus.resetCount > 255) sysStatus.resetCount = 0;
  sysStatusWriteNeeded = true;
}


void watchdogISR()
{
  watchdogFlag = true;
}

void petWatchdog()
{
  digitalWrite(donePin, HIGH);                                                              // Pet the watchdog
  digitalWrite(donePin, LOW);
  watchdogFlag = false;
  if (Particle.connected && sysStatus.verboseMode) publishQueue.publish("Watchdog","Petted",PRIVATE);
}

void keepAliveMessage() {
  Particle.publish("*", PRIVATE,NO_ACK);
}

void sendEvent()
{
  char data[512];                 
  snprintf(data, sizeof(data), "{\"sensorOne\":%4.1f, \"sensorTwo\":%4.1f,  \"sensorThree\":%4.1f,  \"sensorFour\":%4.1f,  \"sensorFive\":%4.1f,\"sensorSix\":%4.1f}", sensorData.sensorOneCurrent,sensorData.sensorTwoCurrent,sensorData.sensorThreeCurrent,sensorData.sensorFourCurrent,sensorData.sensorFiveCurrent,sensorData.sensorSixCurrent);
  publishQueue.publish("powermonitoring_hook", data, PRIVATE);
  dataInFlight = true;                                                                      // set the data inflight flag
  webhookTimeStamp = millis();
}

void UbidotsHandler(const char *event, const char *data) {            // Looks at the response from Ubidots - Will reset Photon if no successful response
  char responseString[64];
    // Response is only a single number thanks to Template
  if (!strlen(data)) {                                                // No data in response - Error
    snprintf(responseString, sizeof(responseString),"No Data");
  }
  else if (atoi(data) == 200 || atoi(data) == 201) {
    snprintf(responseString, sizeof(responseString),"Response Received");
    sysStatus.lastHookResponse = Time.now();                          // Record the last successful Webhook Response
    sysStatusWriteNeeded = true;
    dataInFlight = false;                                             // Data has been received
  }
  else {
    snprintf(responseString, sizeof(responseString), "Unknown response recevied %i",atoi(data));
  }
  publishQueue.publish("Ubidots Hook", responseString, PRIVATE);
}

// Function to Blink the LED for alerting. 
void blinkLED(int LED)                                                                      // Non-blocking LED flashing routine
{
  const int flashingFrequency = 1000;
  static unsigned long lastStateChange = 0;

  if (millis() - lastStateChange > flashingFrequency) {
    digitalWrite(LED,!digitalRead(LED));
    lastStateChange = millis();
  }
}

// These are the particle functions that allow you to configure and run the device
// They are intended to allow for customization and control during installations
// and to allow for management.


int measureNow(String command) // Function to force sending data in current hour
{
  if (command == "1") {
    state = MEASURING_STATE;
    return 1;
  }
  else return 0;
}

int setVerboseMode(String command) // Function to force sending data in current hour
{
  if (command == "1")
  {
    sysStatus.verboseMode = true;
    publishQueue.publish("Mode","Set Verbose Mode",PRIVATE);
    sysStatusWriteNeeded = true;
    return 1;
  }
  else if (command == "0")
  {
    sysStatus.verboseMode = false;
    publishQueue.publish("Mode","Cleared Verbose Mode",PRIVATE);
    sysStatusWriteNeeded = true;
    return 1;
  }
  else return 0;
}

void publishStateTransition(void)
{
  char stateTransitionString[40];
  snprintf(stateTransitionString, sizeof(stateTransitionString), "From %s to %s", stateNames[oldState],stateNames[state]);
  oldState = state;
  if(Particle.connected()) publishQueue.publish("State Transition",stateTransitionString, PRIVATE);
}

int setThirdPartySim(String command) // Function to force sending data in current hour
{
  if (command == "1")
  {
    sysStatus.thirdPartySim = true;
    Particle.keepAlive(sysStatus.keepAlive);                                                // Set the keep alive value
    keepAliveTimer.changePeriod(sysStatus.keepAlive*1000);                                  // Will start the repeating timer
    if (Particle.connected()) publishQueue.publish("Mode","Set to 3rd Party Sim", PRIVATE);
    sysStatusWriteNeeded = true;
    return 1;
  }
  else if (command == "0")
  {
    sysStatus.thirdPartySim = false;
    if (Particle.connected()) publishQueue.publish("Mode","Set to Particle Sim", PRIVATE);
    sysStatusWriteNeeded = true;
    return 1;
  }
  else return 0;
}

int setKeepAlive(String command)
{
  char * pEND;
  char data[256];
  int tempTime = strtol(command,&pEND,10);                                                  // Looks for the first integer and interprets it
  if ((tempTime < 0) || (tempTime > 1200)) return 0;                                        // Make sure it falls in a valid range or send a "fail" result
  sysStatus.keepAlive = tempTime;
  Particle.keepAlive(sysStatus.keepAlive);                                                // Set the keep alive value
  keepAliveTimer.changePeriod(sysStatus.keepAlive*1000);                                  // Will start the repeating timer
  snprintf(data, sizeof(data), "Keep Alive set to %i sec",sysStatus.keepAlive);
  publishQueue.publish("Keep Alive",data, PRIVATE);
  sysStatusWriteNeeded = true;                                                           // Need to store to FRAM back in the main loop
  return 1;
}

void getBatteryContext() 
{
  const char* batteryContext[7] ={"Unknown","Not Charging","Charging","Charged","Discharging","Fault","Diconnected"};
  // Battery conect information - https://docs.particle.io/reference/device-os/firmware/boron/#batterystate-
  // sysStatus.batteryState = System.batteryState();
  snprintf(batteryContextStr, sizeof(batteryContextStr),"%s", batteryContext[sysStatus.batteryState]);
  sysStatusWriteNeeded = true;
}

/*
  * These functions change the values of the constant. 
*/

// Function Prototypes
int setConstantOne(String command){
  sensorConstants.sensorOneConstant = command.toFloat();
  publishQueue.publish("Constant One Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantTwo(String command){
  sensorConstants.sensorTwoConstant = command.toFloat();
  publishQueue.publish("Constant Two Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantThree(String command){
  sensorConstants.sensorThreeConstant = command.toFloat();
  publishQueue.publish("Constant Three Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantFour(String command){
  sensorConstants.sensorFourConstant = command.toFloat();
  publishQueue.publish("Constant Four Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantFive(String command){
  sensorConstants.sensorFiveConstant = command.toFloat();
  publishQueue.publish("Constant Five Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantSix(String command){
  sensorConstants.sensorSixConstant = command.toFloat();
  publishQueue.publish("Constant Six Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

// This function updates the constants value string in the console. 
void updateConstantValues()
{   
    snprintf(sensorOneConstantStr,sizeof(sensorOneConstantStr),"CT One: %3.1f", sensorConstants.sensorOneConstant);
    snprintf(sensorTwoConstantStr,sizeof(sensorTwoConstantStr),"CT Two %3.1f", sensorConstants.sensorTwoConstant);
    snprintf(sensorThreeConstantStr,sizeof(sensorThreeConstantStr),"CT Three: %3.1f", sensorConstants.sensorThreeConstant);
    snprintf(sensorFourConstantStr,sizeof(sensorFourConstantStr),"Sensor Four Constant : %3.1f", sensorConstants.sensorFourConstant);
    snprintf(sensorFiveConstantStr,sizeof(sensorFiveConstantStr),"Sensor Five Constant : %3.1f", sensorConstants.sensorFiveConstant);
    snprintf(sensorSixConstantStr,sizeof(sensorSixConstantStr),"Sensor Six Constant : %3.1f", sensorConstants.sensorSixConstant);
    constantsStatusWriteNeeded = true;                                                         // This function is called when there is a change so, we need to update the FRAM
} 

/* 
  EnableSensor (String Sensor)
  This function takes in the sensor number as integer and enables or disable the sensor accordingly.
*/
int enableSensor(String Sensor){
  char * pEND;
  char data[256];
  int tempSensor = strtol(Sensor,&pEND,10);                                                  // Looks for the first integer and interprets it
  if ((tempSensor < 1) || (tempSensor >7) ) return 0;
 
  if (tempSensor == 1){
    sysStatus.sensorOneConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor One");
    sysStatusWriteNeeded = true;  
    return 1;     
  } else if (tempSensor == 2){
    sysStatus.sensorTwoConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Two");
    sysStatusWriteNeeded = true;   
    return 1;    
  }
  else if (tempSensor == 3){
    sysStatus.sensorThreeConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Three");
    sysStatusWriteNeeded = true;       
    return 1;
  }
  else if (tempSensor == 4){
    sysStatus.sensorFourConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Four");
    sysStatusWriteNeeded = true;     
    return 1;  
  }
  else if (tempSensor == 5){
    sysStatus.sensorFiveConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Five");
    sysStatusWriteNeeded = true;    
    return 1;   
  }
  else if (tempSensor == 6){
    sysStatus.sensorSixConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Six");
    sysStatusWriteNeeded = true; 
    return 1;      
  }
}

// These are the functions that are part of the takeMeasurements call

bool takeMeasurements() 
{
    sensorData.validData = false;
    
    getBatteryContext();     

    if (sysStatus.sensorOneConnected) sensorData.sensorOneCurrent =   emon1.calcIrms(1480);
    if (sysStatus.sensorTwoConnected) sensorData.sensorTwoCurrent =   emon2.calcIrms(1480);
    if (sysStatus.sensorThreeConnected) sensorData.sensorThreeCurrent=  emon3.calcIrms(1480);
    if (sysStatus.sensorFourConnected) sensorData.sensorFourCurrent =  emon4.calcIrms(1480);
    if (sysStatus.sensorFiveConnected) sensorData.sensorFiveCurrent =  emon5.calcIrms(1480);               
    if (sysStatus.sensorSixConnected) sensorData.sensorSixCurrent =   emon6.calcIrms(1480);

    // Indicate that this is a valid data array and store it
    sensorData.validData = true;
    sensorData.timeStamp = Time.now();
    sensorDataWriteNeeded = true;
    return 1;

  }


  void getBatteryCharge()
{
  voltage = analogRead(BATT) * 0.0011224;
  snprintf(batteryString, sizeof(batteryString), "%3.1f V", voltage);
}
