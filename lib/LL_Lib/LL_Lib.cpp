#include <Arduino.h>
#include <Print.h>
#include <HardwareSerial.h>
#include <Stream.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#include "LL_Lib.h"

void LL_Log_c::begin(uint32_t serialRate, uint16_t tcpPort) {
  if(serialRate != 0) {
    Serial.begin(serialRate);
    _ptrlogSerial = &Serial;
  } 
  _logTCPServerPort = tcpPort;
  _PreviousWifiConnectionState = false;
}

size_t LL_Log_c::write(uint8_t v) {
  return write(&v, 1);
}

size_t LL_Log_c::write(const uint8_t *buffer, size_t size) {
  if(_ptrlogSerial) _ptrlogSerial->write(buffer, size);
  if(_logTCPClient.connected()) _logTCPClient.write(buffer, size);
  return size;
}

void LL_Log_c::receiveChar(char c) {
  if(_receiveLineAvailable == false) {
    if(c >= ' ') {
      if(_receiveLineIndex < (RECEIVELINEBUFSIZE - 2)) {
        _receiveLineBuf[_receiveLineIndex++] = c;
      } else {
        _receiveLineBuf[_receiveLineIndex] = 0;
        _receiveLineAvailable = true;
        _receiveLineIndex = 0;
      }
    }
    if((c == 0x0d) || (c == 0x0a)) {
      if(_receiveLineIndex > 0) {
        _receiveLineBuf[_receiveLineIndex] = 0;
        _receiveLineAvailable = true;
        _receiveLineIndex = 0;        
      }
    }
  }
}

bool LL_Log_c::receiveLineAvailable() {
  bool ret = _receiveLineAvailable;
  if(ret) {
    strncpy(receiveLine, _receiveLineBuf, RECEIVELINEBUFSIZE);
    _receiveLineAvailable = false;
  }
  return(ret);
}

void LL_Log_c::update() {

  // Check Wifi connection state change
  bool isWifiConnected = WiFi.isConnected();
  if(_PreviousWifiConnectionState != isWifiConnected) {
    if(isWifiConnected) {
      // Wifi now available after it was disconnected
      if(_ptrlogTCPServer != 0) {
        _ptrlogTCPServer->close();
        delete _ptrlogTCPServer;
        _ptrlogTCPServer = NULL;
      }
      if(_logTCPServerPort != 0) {
        // Start TCP Server
        WiFiServer *srv = new WiFiServer(_logTCPServerPort);
        srv->begin(_logTCPServerPort);
        _ptrlogTCPServer = srv;
      }

    } else {
      // Wifi now went offline
       if(_ptrlogTCPServer != 0) {
        _logTCPClientConnected = false;
        _logTCPClient.stop(); // .abort();
        _ptrlogTCPServer->close();
        delete _ptrlogTCPServer;
        _ptrlogTCPServer = NULL;
      }     
    }
    _PreviousWifiConnectionState = isWifiConnected;
  }

  // Check if WiFi is connected 
  if(isWifiConnected) {
    if(_ptrlogTCPServer != 0) {
      if (_ptrlogTCPServer->hasClient()) {
        _logTCPClient = _ptrlogTCPServer->accept();
        Serial.println("New client");
        _logTCPClient.println("TCP Log opened.");
        _logTCPClientConnected = true;
      }
      if(_logTCPClient.available()) {
        receiveChar(_logTCPClient.read());
      }
    }
  }

  if(_ptrlogSerial) {
    if(_ptrlogSerial->available()) {
      receiveChar(_ptrlogSerial->read());
    }
  }

}

LL_Log_c LL_Log;
