#pragma once

#include <Arduino.h>

void connectWifi(const char* ssid, const char* password, const char* hostname);
void setupOTA(const char* hostname);
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);

extern bool OTAinProgress;

class touchProcessor {
  public:
    touchProcessor(int pin) : _pin(pin) {
      _touchAvgValue = 0;
      _touched = false;
    }
    void update() {
      _lastTouchValue = touchRead(_pin)/32;
      if(_touchAvgValue == 0) _touchAvgValue = _lastTouchValue;
      _touchAvgValue = _touchAvgValue * 0.98 + _lastTouchValue * 0.02;
      _touchValue = _lastTouchValue - _touchAvgValue;
      if(_touchValue > 20) {
        _touched = true;
      } else {
        _touched = false;
      }
    }
    bool isTouched() {
      return _touched;
    }
    int getTouchValue() {
      return _touchValue;
    }
  private:
    int _pin;
    uint32_t _lastTouchValue;
    uint32_t _touchAvgValue;
    uint32_t _touchValue;
    bool _touched;
};