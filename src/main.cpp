#include <Arduino.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>   
#include <MQTTClient.h>
#include <ArduinoOTA.h>
#include <pw.h>

WiFiManager wifiman;
MQTTClient client;
WiFiClient net;


enum OpModes
{
  MODE_INIT, // 0
  MODE_STANDBY,
  MODE_OTA,
  MODE_START,
  MODE_PUMP,
  MODE_CALIBRATE,
  MODE_MEASURE,
  MODE_RELEASE,
  MODE_ERR // 7
};
int OPMODE = 0;  // switch variable


// ############## timestamps for periodic functions
uint64_t lastPump = 0;  // timestamp for pump actuation
uint64_t lastMeasurement = 0;  // timestamp for last pressure measurement
uint64_t lastStatus = 0;  // timestamp for periodic status messages

// ############## timestamps for periodic functions

// ############## config vars
uint64_t statusInterval = 5000;
// ############## config vars

// ############## measurement
uint32_t pressureval = 0;  // value for measured differential pressure
uint32_t pressurevalold = 0 ;  // value for the old measured pressure

uint32_t pressurecal = 0;  // value for calibrated pressure
// ############## measurement

bool interlock = false;  // switch to prevent all  
bool otaflag = false;  // flag for enabling OTA updates
bool busyflag = false;  // flag for signaling device busy
bool measureflag = false;  // flag for starting measurement

String statePublishTopic = "Cistern/state";
String measurePublishTopic = "Cistern/values";
String EspStatus = "INIT";


void clientstatepub(String message){
  client.publish(statePublishTopic,message);
}

void clientvalpub(String message){
  client.publish(measurePublishTopic,message);
}

void statemachine(){
  switch (OPMODE)
  {
  case MODE_INIT:
    // check for inputs deactivated
    EspStatus = "Initializing";
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
    }else if (measureflag)
    {
      EspStatus = "Starting Measurement";
      statusPrinter(1);
      OPMODE = MODE_START;
    }    
    break;
  case MODE_OTA:
    otaflag = false;
    ArduinoOTA.handle(); 
    break;
  case MODE_START:
    // enable solenoid valve for some time to release all pressure in the system
    // calibrate zero
    // disable solenoid valve
    // enable pumping to pressurize system for some time
    // disable pump
    // measure pressure
    // calculate volume
    // publish calculated value to MQTT
    break;
  case MODE_RELEASE: 
    break;
    
  default:
    break;
}
}

void connectMQTT(){
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nconnecting to MQTT broker...");
  while (!client.connect("D1 Mini Cistern", mqttusr, mqttpass)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");

  client.subscribe("Cistern/Cmnd");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: ");
  Serial.println(topic);
  Serial.print("payload: |");
  Serial.print(payload);
  Serial.print("|");
  payload.trim();
  Serial.print("trimmed payload: |");
  Serial.print(payload);
  Serial.print("|");
  
  if (payload == "Go" and !interlock)  // if interlock is not set, we can enable measurement
  {
    Serial.println("Measurement commencing");
    interlock = true;  // lock actuating again
    OPMODE = MODE_START;
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
void statusPrinter(int force)
{
  long now = millis();
  if (now - lastStatus >= statusInterval or force == 1)
  {

    lastStatus = now;
    Serial.print("<{\"Status_Mega\":\"");
    Serial.print(EspStatus);
    Serial.print("\",\"Uptime\":\"");
    Serial.print(now);
    Serial.println("\"}>");
    clientstatepub(EspStatus);
    if (force = 1)
    {
      statusInterval = 5000;
    }
    
  }
  else if (force == 2)
  {
    Serial.print("<{\"Status_Mega\":\"");
    Serial.print(EspStatus);
    Serial.print("\",#\"Uptime\":\"");
    Serial.print(now);
    Serial.println("\"}>");
    clientstatepub("Device busy");
    busyflag = false;
  
    
  }
}
void setup() {
  // put your setup code here, to run once:
  // start Serial
  Serial.begin(9600);
  // init Wifimanager
  // autoconnect from EEprom, else enable "CisternAP"
  wifiman.autoConnect("CisternAP");
  Serial.print("Wifi connected");
  // init OTA
  ArduinoOTA.setPassword(otapass);  // set OTA pass for uploading 

  ArduinoOTA.onStart([](){
    clientstatepub("OTA_STARTING");
    String type;
    interlock = true;  // prevent callbacks interrupting the operation
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("Start updating" + type);
  });
  ArduinoOTA.onEnd([](){
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](uint32_t progress, uint32_t total){
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
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
    }
  });
  
  Serial.println(net.localIP());
  // init mqtt client
  client.begin("192.168.178.81", 1883, net);
  client.onMessage(messageReceived);
  client.setKeepAlive(60);
  connectMQTT();
}

void loop() {
  client.loop();
  if (!client.connected()){
    connectMQTT();
  }
  if (busyflag and !otaflag)
  {
    EspStatus = "Device busy";
    statusPrinter(2);  // force publishing status
  }
  else if(!otaflag)  // do not do this if ota in progress
  {
    statusPrinter(0);
  }
  
  statemachine();

  // put your main code here, to run repeatedly:
}