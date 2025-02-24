#include "helper.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>

#include "LL_Lib.h"

bool OTAinProgress = false;

void connectWifi(const char* ssid, const char* password, const char* hostname) {
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int tries = 0;
  LL_Log.println("Connecting to WiFi..");
  while ((WiFi.status() != WL_CONNECTED) && (tries < 20)) {
    delay(500);
    LL_Log.print(".");
    tries++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    LL_Log.print("Connected to ");
    LL_Log.println(ssid);
    LL_Log.print("IP address: ");
    LL_Log.println(WiFi.localIP());
    setupOTA(hostname);
  } else {
    LL_Log.println("Connection failed.");
  }
  
}

void setupOTA(const char* hostname) {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    LL_Log.println("Start updating " + type);
    OTAinProgress = true;
  });
  ArduinoOTA.onEnd([]() {
    LL_Log.println("\r\nOTA Done.");
    OTAinProgress = false;
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    LL_Log.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      LL_Log.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      LL_Log.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      LL_Log.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      LL_Log.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      LL_Log.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  
  MDNS.begin(hostname);
  MDNS.addService("debug", "tcp", 2222);
}





float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  // Check if x is outside the input range
  if (x <= in_min) return out_min;
  if (x >= in_max) return out_max;

  // Calculate the proportion of x relative to the input range
  float proportion = (x - in_min) / (in_max - in_min);

  // Map the proportion to the output range and return the result
  return (proportion * (out_max - out_min)) + out_min;
}