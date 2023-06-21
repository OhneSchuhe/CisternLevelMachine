#include <ArduinoOTA.h>
void setupota(const char otapass[])
{
      // init OTA
  ArduinoOTA.setPassword(otapass); // set OTA pass for uploading

  ArduinoOTA.onStart([]()
                     {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("Start updating" + type); });
  ArduinoOTA.onEnd([]()
                   {
                    Serial.println("\nEnd"); 
                    ESP.reset();
                   });
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
}

void beginota()
{
  ArduinoOTA.begin(); 
}
void handleota()
{
  ArduinoOTA.handle();
}