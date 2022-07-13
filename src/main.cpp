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
uint64_t lastmeasurement = 0;  // timestamp for last pressure measurement

// ############## timestamps for periodic functions

// measurement
uint32_t pressureval = 0;  // value for measured differential pressure
uint32_t pressurevalold = 0 ;  // value for the old measured pressure

uint32_t pressurecal = 0;  // value for calibrated pressure
// measurement

bool interlock = false;  // switch to prevent all  
bool otaflag = false;  // flag for enabling OTA updates
bool otainit = false;

void statemachine(){
  switch (OPMODE)
  {
  case MODE_INIT:
    // check for inputs deactivated
    if (!interlock)
    {
      OPMODE = MODE_STANDBY;
    }
    
    break;
  case MODE_STANDBY:
    // check for 
    
    
    break;
  case MODE_OTA:
    ArduinoOTA.handle(); 
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
  Serial.println("payload:");
  Serial.println(payload);
  
  if (payload = "Go" and !interlock)  // if interlock is not set, we can enable measurement
  {
    interlock = true;  // lock actuating again
    OPMODE = MODE_START;
  }else if (payload = "OTA_EN" and !interlock)
  {
    interlock = true;
    OPMODE = MODE_OTA;
    ArduinoOTA.begin();
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

  statemachine();

  // put your main code here, to run repeatedly:
}