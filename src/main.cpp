#include <Arduino.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>   
#include <MQTTClient.h>
#include <pw.h>

WiFiManager wifiman;
MQTTClient client;
WiFiClient net;


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
  

}

void setup() {
  // put your setup code here, to run once:
  // start Serial
  Serial.begin(9600);
  // init Wifimanager
  
  // autoconnect from EEprom, else enable "CisternAP"
  wifiman.autoConnect("CisternAP");
  Serial.print("Wifi connected");
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
  // put your main code here, to run repeatedly:
}