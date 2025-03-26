// hackffm_badge.h
#ifndef HACKFFM_BADGE_LIB_H
#define HACKFFM_BADGE_LIB_H

#include <Arduino.h>
#include <elapsedMillis.h>
#include <Freenove_WS2812_Lib_for_ESP32.h>
#include <LittleFS.h>
#include <FS.h>
#include <U8g2lib.h>
#include "Face.h"
#include "LL_Lib.h"

extern U8G2_DISPLAYTYPE u8g2;
extern U8G2LOG u8g2log;
#define U8G2_DISP_WIDTH  128 
#define U8G2_DISP_HEIGHT 64

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);

class touchProcessor {
  public:
    touchProcessor(int pin) : _pin(pin) {
      _touchAvgValue = 0;
      _touched = false;
    }
    void update() {
      if(_pin < 0) return;
      _lastTouchValue = touchRead(_pin)/16;
      if(_touchAvgValue == 0) _touchAvgValue = _lastTouchValue;
      _touchAvgValue = _touchAvgValue * 0.95 + _lastTouchValue * 0.05;
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
    int getLastTouchValue() {
      return _lastTouchValue;
    }
  private:
    int _pin = -1;
    uint32_t _lastTouchValue;
    uint32_t _touchAvgValue;
    uint32_t _touchValue;
    bool _touched;
};

class HackFFMBadgeLib {
  public:
    void begin();
    void update();  // Updates touch
    void powerOff();

    void setBoardLED(uint32_t rgb);  // set the LED on the little ESP32-S3 board
    void setBoardLED(uint8_t r, uint8_t g, uint8_t b);

    void setAntennaLED(uint32_t rgb);
    void setAntennaLED(uint8_t r, uint8_t g, uint8_t b);

    void setPowerAudio(bool on);  // Enables or disables the audio amplifier (amps draws high current from battery!)

    uint32_t but0PressedSince(); // 0 = not pressed
    uint32_t but0PressedFor();   // 0 = no event or still pressed, otherwise time in ms of last press, cleared after read
    
    bool isNewTouchDataAvailable() { bool r = touchUpdated; touchUpdated = false; return r; }

    void listDir(const char *dirname = "/", uint8_t levels = 1);

    void writeTone(uint32_t freq = 0 /* 0 to stop */, uint32_t duty = 8);

    Face& face() { return *face_; }
    bool faceActive = false;
    void drawFace() { face_->Draw(); }

     /* Special characters:
      * 0x0a \n: new line
      * $: Escape, next char defines action
      *    $: prints $
      *    c,l,r: center/left/right aligned
      *    0..9,a,b,d,e,f,g: set fonts
      *    !: clear display
      */
    void drawString(const char *str, int x = 0, int y = 0, int dy = 2, bool noDraw = false); 

    void drawLog();

    void drawBMP(const char *filename);

  // quasi-private, usually no need to use directly 
    Freenove_ESP32_WS2812* freenoveAntennaLED = nullptr;  
    int pinOledSda, pinOledSck, pinPwrHold, pinAudio, pinAudioEn,
      pinTouchLU, pinTouchLD, pinTouchRU, pinTouchRD, pinNeoPixel,
      pinExtA, pinExtB, pinBoardRGB, pinBoardTx, pinBoardRx; 
    static const int NUM_TOUCH_PINS = 4;
    touchProcessor touch[NUM_TOUCH_PINS] = {-1, -1, -1, -1}; // LU, LD, RU, RD

    void connectWifi(const char* ssid, const char* password, const char* hostname);
    void setupOTA(const char* hostname);

    bool OTAinProgress = false;

  private:
    void detectHardware();
    static const int PINS_PER_VARIANT = 15; 
    int pinVariantIndex = 0; 
    static const int pinVariants[][PINS_PER_VARIANT]; // Pin-Matrix
    static const int NUM_PIN_VARIANTS;
  
    int boardType = 0; // 0: unknown, 1: blue, 2: black (detection unreliable!)
    static const char* const boardTypeTexts[]; 
    Freenove_ESP32_WS2812* freenoveBoardLED = nullptr; 
    uint8_t OledAddress = 0;

    elapsedMillis lastTouchRead = 0;
    bool touchUpdated = false;
    void setPowerHold(bool on);
    void initOLED();
    Face* face_ = nullptr;

    elapsedMillis but0CurrentPressedMs = 0;
    uint32_t but0WasPressedForMs = 0;
    bool but0LastPressed = false;
    
};

extern HackFFMBadgeLib HackFFMBadge;
extern HackFFMBadgeLib& Badge;  // alias for less writing but less HackFFM advertizing :-)

#endif // HACKFFM_BADGE_LIB_H