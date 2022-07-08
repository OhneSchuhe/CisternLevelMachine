#include <Arduino.h>
#include <WiFiManager.h>   
#include <MQTTClient.h>

void setup() {
  // put your setup code here, to run once:
  // start Serial
  Serial.begin(115200);
  // init Wifimanager
  WiFiManager wifiman;
  // autoconnect from EEprom, else enable "CisternAP"
  wifiman.autoConnect("CisternAP");
  Serial.print("Wifi connected");
  // init mqtt client
  MQTTClient client;
  Serial.print("\nconnecting to MQTT broker...");
  while (!client.connect("arduino", "public", "public")) {
    Serial.print(".");
    delay(1000);
  }

}

void loop() {
  // put your main code here, to run repeatedly:
}