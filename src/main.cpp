#include <Arduino.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <WiFiManager.h>
#include <MQTTClient.h>
#include <ArduinoOTA.h>
#include "HX711.h"
#include <EEPROM.h>
#include <pw.h>

WiFiManager wifiman;
MQTTClient client;
WiFiClient net;
HX711 psensor;




enum opModes
{
  MODE_INIT, // 0
  MODE_STANDBY,
  MODE_OTA,
  MODE_START,
  MODE_CALIBRATE,
  MODE_SELFTEST,
  MODE_MEASURE,
  MODE_RELEASE,
  MODE_ERR // 7
};
uint8_t OPMODE = 0; // switch variable

// IOS
uint8_t pin_pump = D6;
uint8_t pin_valve = D8;
uint8_t pin_dout = D5;
uint8_t pin_clk = D0;
// IOS


// ############## timestamps for periodic functions
uint64_t lastPump = 0;        // timestamp for pump actuation
uint64_t lastMeasurement = 0; // timestamp for last pressure measurement
uint64_t lastSelfTest = 0;  // timestamp for self test
uint64_t lastcalibration = 0;  // timestamp for calibration
uint64_t lastStatus = 0;      // timestamp for periodic status messages
uint32_t lastInit = 0;        // timestamp for init routine
// ############## timestamps for periodic functions

// ############## config vars
uint64_t statusInterval = 5000;
uint64_t selfTestInterval = 5000;
uint64_t calibrationInterval = 5000;
uint64_t calibrationTimeout = 300000;  // 5m timeout for calibration
uint16_t psensorgain = 64;  // gain for the HX711 module. possible vars are 64 and 128 for channel A
uint8_t eepromscaleadress = 0;  // address for storing the psensor scaling factor in eeprom
uint32_t psensorscale = 0;  // scaling factor for the calibrated scale
// ############## config vars

// ############## measurement
uint32_t pressureval = 0;    // value for measured differential pressure
uint32_t pressurevalold = 0; // value for the old measured pressure

uint32_t pressurecal = 0; // value for calibrated pressure
// ############## measurement

// ############## flags
bool interlock = false;   // switch to prevent all
bool otaflag = false;     // flag for enabling OTA updates
bool busyflag = false;    // flag for signaling device busy
bool measureflag = false; // flag for starting measurement
bool selftestflag = false;  // flag for self testing
bool calibrateflag = false;  // flag for starting calibration
bool resetflag = false;  // flag for resetting from error
// ############## flags
String statePublishTopic = "Cistern/state";
String measurePublishTopic = "Cistern/values";
String commandTopic = "Cistern/Cmnd/*";
String EspStatus = "INIT";

uint8_t dios[] = 
{
  pin_pump,
  pin_valve
};

void psensordebug()
{
   if (psensor.is_ready()) {
    long reading = psensor.read();
    Serial.print("HX711 reading: ");
    Serial.println(reading);
  } else {
    Serial.println("HX711 not found.");
  }
}

 uint32_t readpsensor()
{
  uint32_t val = 0;
  // if sensor is ready, read value
  if (psensor.is_ready()) {
    val = psensor.read_average(20);
  } else {  // else set value to 0
    val = 0;
  }
  return val;
}


void clientstatepub(String message)
{
  client.publish(statePublishTopic, message);
}

void clientvalpub(String message)
{
  client.publish(measurePublishTopic, message);
}
void statusPrinter(int force)
{
  long now = millis();
  if ((now - lastStatus >= statusInterval) or force == 1)
  {

    lastStatus = now;
    Serial.print("<{\"Status_Cistern\":\"");
    Serial.print(EspStatus);
    Serial.print("\",\"Uptime\":\"");
    Serial.print(now);
    Serial.println("\"}>");
    clientstatepub(EspStatus);
    if (force == 1)
    {
      statusInterval = 5000;
    }
    //debug
    // psensordebug();
    //debug
  }
  else if (force == 2)
  {
    Serial.print("<{\"Status_Cistern\":\"");
    Serial.print(EspStatus);
    Serial.print("\",#\"Uptime\":\"");
    Serial.print(now);
    Serial.println("\"}>");
    clientstatepub("Device busy");
    busyflag = false;
  }
}
void statemachine()
{
  switch (OPMODE)
  {
  case MODE_INIT:
    // check for inputs deactivated
    EspStatus = "Initializing";
    if (lastInit == 0) 
    {
      lastInit = millis();  // get timestamp
    } 
     
    if (!interlock)
    {
      EspStatus = "Standby";
      OPMODE = MODE_STANDBY;
    }
    
    break;
  case MODE_STANDBY:
    // check for start flag

    statusInterval = 300000;
    if (otaflag)
    {
      EspStatus = "OTA_RDY";
      statusPrinter(1);
      OPMODE = MODE_OTA;
    }
    else if (measureflag)
    {
      EspStatus = "Starting Measurement";
      statusPrinter(1);
      lastSelfTest = millis();  // create timestamp
      OPMODE = MODE_SELFTEST;
    }
    else if (calibrateflag)
    {
      EspStatus = "Starting Calibration";
      statusPrinter(1);
      lastcalibration = millis();
      OPMODE = MODE_CALIBRATE;
    }
    
    break;
  case MODE_OTA:
  {
    otaflag = false;
    ArduinoOTA.handle();
    break;
  }
  case MODE_SELFTEST:
   { // close solenoid valve
    statusInterval = 1000;
    long now = millis();
    EspStatus = "Self Test";
    if (!selftestflag)
    {
      psensor.power_up();
      digitalWrite(pin_valve, HIGH);  // close valve
      for (size_t i = 0; i < 5; i++)
      {
        Serial.print("reading psensor");
        pressureval = readpsensor();  // read current pressure as baseline
        if (pressureval != 0)
        {
          break;
        } 
      }
      digitalWrite(pin_pump, HIGH);  // enable pump
      selftestflag = true;
    }
    
    
    if (now - lastSelfTest >= selfTestInterval )
    {
      digitalWrite(pin_pump, LOW);  // disable pump
      if (readpsensor() > pressureval)
      {
        // time stamp self test done
        // for releasing all the pressure from the system
        EspStatus = "Self Test pass";
        statusPrinter(1);
        OPMODE = MODE_START;
        selftestflag = false;
      }
      else
      {
        EspStatus = "Self Test fail";
        statusPrinter(1);
        OPMODE = MODE_ERR;
        selftestflag = false;
      }
      
     
      digitalWrite(pin_valve, LOW);  // open valve
    }
    
    // activate pump
    // measure pressure for a definec timeframe
    // if pressure rises, system is able to function
    // else go into Error mode
    break;
}
  case MODE_CALIBRATE:
  {
    long now = millis();
    digitalWrite(pin_valve,HIGH);  // close valve
    digitalWrite(pin_pump,HIGH);  // activate pump to pressurize the system
    if (now - lastcalibration < calibrationInterval)
    {
      psensor.set_scale();  // set scale without having calibrated value
      psensor.tare();  // tare scale
      psensor.get_units(10);  // get untared units
    }
    if (now - lastcalibration < calibrationTimeout)
    {
      /* code */
    }
    
    // https://github.com/bogde/HX711#how-to-calibrate-your-load-cell
    // call set_scale() without parameter
    // call tare() without parameter
    // call get_units(10)
    // wait for input of measured height of the water level in the cistern via MQTT
    // calculate the parameter for set_scale()
    // TODO: add a method of checking calculated height against measured height and adjust scale parameter accordingly
    calibrateflag = false;
    break;
  }
  case MODE_START:
{    // enable solenoid valve for some time to release all pressure in the system
    // calibrate zero
    // disable solenoid valve
    // enable pumping to pressurize system for some time
    // disable pump
    // measure pressure
    // calculate volume
    // publish calculated value to MQTT


    break;
}
  case MODE_RELEASE:
  {
    psensor.power_down();
      break;
  }
  case MODE_ERR:
    {statusInterval = 2000;
    EspStatus = "Error State";
    if (resetflag)
    {
      OPMODE = MODE_INIT;
    }
    
    break;}
  default:
   { break;}
  }
}

void connectMQTT()
{
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nconnecting to MQTT broker...");
  while (!client.connect("D1 Mini Cistern", mqttusr, mqttpass))
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");

  client.subscribe(commandTopic);
}

void messageReceived(String &topic, String &payload)
{
  Serial.println("incoming: ");
  Serial.println(topic);
  Serial.print("payload: |");
  Serial.print(payload);
  Serial.print("|");
  payload.trim();
  Serial.print("trimmed payload: |");
  Serial.print(payload);
  Serial.print("|");

  if (payload == "Go" and !interlock) // if interlock is not set, we can enable measurement
  {
    Serial.println("Measurement commencing");
    interlock = true; // lock actuating again
    measureflag = true;
  }
  else if (payload == "OTA_EN" and !interlock)
  {
    Serial.println("Received OTA Command, enabling OTA Handling");
    interlock = true;
    otaflag = true;
    ArduinoOTA.begin();
  }
  else if (interlock)
  {
    // cmnd was received but device is busy
    busyflag = true;
  }
}

void setup()
{
  // put your setup code here, to run once:
  // start Serial
  Serial.begin(57600);
  // init EEPROM
  EEPROM.begin(8);
  // get scale factor from EEPROM
  EEPROM.get(eepromscaleadress, psensorscale);
  // init dios
  for (uint8_t i = 0; i < sizeof(dios)/sizeof(*dios); i++)
  {
    pinMode(dios[i],OUTPUT);
  }
  // init HX711 module
  psensor.begin(pin_dout,pin_clk,psensorgain);
  // init Wifimanager
  // autoconnect from EEprom, else enable "CisternAP"
  wifiman.autoConnect("CisternAP");
  Serial.print("Wifi connected");
  // init OTA
  ArduinoOTA.setPassword(otapass); // set OTA pass for uploading

  ArduinoOTA.onStart([]()
                     {
    clientstatepub("OTA_STARTING");
    String type;
    interlock = true;  // prevent callbacks interrupting the operation
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("Start updating" + type); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](uint32_t progress, uint32_t total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    } });

  Serial.println(net.localIP());
  Serial.printf("Wifi Strength[%d]: " ,  WiFi.RSSI());
  // init mqtt client
  client.begin("192.168.178.81", 1883, net);
  client.onMessage(messageReceived);
  client.setKeepAlive(60);
  connectMQTT();
}


void loop()
{
  client.loop();
  if (!client.connected())
  {
    connectMQTT();
  }
  if (busyflag and !otaflag)
  {
    EspStatus = "Device busy";
    statusPrinter(2); // force publishing status
  }
  else if (!otaflag) // do not do this if ota in progress
  {
    statusPrinter(0);
  }
  statusPrinter(0);
  statemachine();

  // put your main code here, to run repeatedly:
}