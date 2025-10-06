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

#include <U8g2lib.h>
extern U8G2LOG u8g2log;

void LL_Log_c::begin(uint32_t serialRate, uint16_t tcpPort) {
  _xMutex = xSemaphoreCreateMutex();
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
  if(xSemaphoreTake(_xMutex, 1000 / portTICK_PERIOD_MS)) {
    if(_ptrlogSerial) _ptrlogSerial->write(buffer, size);
    for(int i=0; i<LL_LOG_MAX_CLIENTS; i++) {
      if(_logTCPClients[i]) {
        if(_logTCPClients[i].connected()) _logTCPClients[i].write(buffer, size);
        delay(1);
      }
    }
    #if defined(U8G2_DISPLAYTYPE)
      if(u8g2logEnabled) u8g2log.write(buffer, size);
    #endif
    xSemaphoreGive(_xMutex);
  }
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
        srv->setNoDelay(true);
        _ptrlogTCPServer = srv;
        Serial.printf("Log-Server started on port %d.\r\n",_logTCPServerPort);
      }

    } else {
      // Wifi now went offline, stop all clients
      if(_ptrlogTCPServer != NULL) {
        _logTCPClientConnected = false;
        for(int i=0; i<LL_LOG_MAX_CLIENTS; i++) {
          if(_logTCPClients[i]) {
            _logTCPClients[i].stop();
            Serial.printf("Log-Client %d stopped.\r\n", i);
          }
        }
        _logTCPClientConnected = false;
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
        bool accepted = false;
        for(int i=0; i<LL_LOG_MAX_CLIENTS; i++) {
          if((!_logTCPClients[i]) || (!_logTCPClients[i].connected())) {
            if(_logTCPClients[i]) _logTCPClients[i].stop();
            #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
            _logTCPClients[i] = _ptrlogTCPServer->accept();
            #else
            _logTCPClients[i] = _ptrlogTCPServer->available();
            #endif
            Serial.printf("Log-Client %d connected.\r\n", i);
            _logTCPClients[i].println("TCP Log opened.");
            accepted = true;
            _logTCPClientConnected = true;
            break;
          }
        }
        if(!accepted) {
          #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
          _ptrlogTCPServer->accept().stop();
          #else
          _ptrlogTCPServer->available().stop();
          #endif
          Serial.println("Log client connection refused - no free slots.");
        }
      }

      // check all clients for available data or disconnections
      bool clientsleft = false;
      for(int i=0; i<LL_LOG_MAX_CLIENTS; i++) {
          if(_logTCPClients[i]) {
            if(!_logTCPClients[i].connected()) {
              _logTCPClients[i].stop(); 
              Serial.printf("Log-Client %d disconnected.\r\n", i);
              continue;           
            }
            clientsleft = true;
            if(_logTCPClients[i].available()) {
              receiveChar(_logTCPClients[i].read());
            }
          }
      }
      _logTCPClientConnected = clientsleft;
    }
  }

  if(_ptrlogSerial) {
    if(_ptrlogSerial->available()) {
      receiveChar(_ptrlogSerial->read());
    }
  }

}

LL_Log_c LL_Log;
