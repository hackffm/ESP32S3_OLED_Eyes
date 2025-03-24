#pragma once

#include <Arduino.h>

void connectWifi(const char* ssid, const char* password, const char* hostname);
void setupOTA(const char* hostname);


extern bool OTAinProgress;

