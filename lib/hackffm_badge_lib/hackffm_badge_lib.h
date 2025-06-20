// hackffm_badge.h
#ifndef HACKFFM_BADGE_LIB_H
#define HACKFFM_BADGE_LIB_H

#include <Arduino.h>
#include <elapsedMillis.h>
//#include <Freenove_WS2812_Lib_for_ESP32.h> // unstable - kills random memory contents!
#include <Adafruit_NeoPixel.h>
#include "FS.h"
#include <LittleFS.h> // For LittleFS
//#include <SPIFFS.h>   // For SPIFFS
//#include <FFat.h>     // For FATFS

#include <U8g2lib.h>
#include "Face.h"
#include "LL_Lib.h"

extern U8G2_DISPLAYTYPE u8g2;
extern U8G2LOG u8g2log;
#define U8G2_DISP_WIDTH  128 
#define U8G2_DISP_HEIGHT 64

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
int utf8_charlen(const char *s);
size_t hex2bytes(const char *hexString, uint8_t *byteArray, size_t byteArrayLen);
void   bytes2hex(const uint8_t *byteArray, size_t byteArrayLen, char *hexString);
bool is_host_usb_device_connected(void);

class touchProcessor {
  public:
    touchProcessor(int pin) : _pin(pin) {
      _touchAvgValue = 0;
      _touched = false;
    }
    void update() {
      if(_pin < 0) return;
      _lastTouchValue = touchRead(_pin)/2;
      if(_touchAvgValue == 0) _touchAvgValue = _lastTouchValue;
      _touchValue = _lastTouchValue - _touchAvgValue;
      _touchAvgValue += ((_touchValue > 0)?1:-1); 
      _touched = (_touchValue > 160)?true:false; 
    }
    bool isTouched() { return _touched; }
    int32_t getTouchValue() { return _touchValue; }
    int32_t getLastTouchValue() { return _lastTouchValue; }
    int32_t getTouchAvgValue() { return _touchAvgValue; }
    void setPin(int pin) {
      _pin = pin;
      _lastTouchValue = 0;
      _touchAvgValue = 0;
      _touchValue = 0;
      _touched = false;
    }
  private:
    int _pin = -1;
    int32_t _lastTouchValue;
    int32_t _touchAvgValue;
    int32_t _touchValue;
    bool _touched;
};

class HackFFMBadgeLib {
  public:
    HackFFMBadgeLib() : boardLED(1, 21), antennaLED(10, -1) {}

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
    float lastTouchX = 0.0;
    float lastTouchY = 0.0;
    float lastTouchLU = 0.0;  // LU = left up
    float lastTouchLD = 0.0;  // LD = left down
    float lastTouchRU = 0.0;  // RU = right up
    float lastTouchRD = 0.0;  // RD = right down
    bool isTouchLU() { return touch[0].isTouched(); }
    bool isTouchLD() { return touch[1].isTouched(); }
    bool isTouchRU() { return touch[2].isTouched(); }
    bool isTouchRD() { return touch[3].isTouched(); }
    bool isTouchLUStrong() { return touch[0].getTouchValue() > 1500; }
    bool isTouchRUStrong() { return touch[2].getTouchValue() > 1500; }

    void listDir(const char *dirname = "/", uint8_t levels = 1);
    bool writeFile(const char *path, const String &data);
    String readFile(const char *path);

    void writeTone(uint32_t freq = 0 /* 0 to stop */, uint32_t duty = 8);

    Face& face() { return *face_; }
    bool faceActive = false;
    void drawFace() { face_->Draw(); }

     /* Special characters:
      * 0x0a \n or $n: new line
      * $: Escape, next char defines action
      *    $: prints $
      *    c,l,r: center/left/right aligned
      *    0..9,a,b,d,e,f,g: set fonts (small to large)
      *    !: clear display
      *    +: move 1px down, -: move 1px up
      */
    void drawString(const char *str, int x = 0, int y = 0, int dy = 2, bool noDraw = false); 

    void drawLog(bool noDraw = false);

    void drawBMP(const char *filename, bool noDraw = false);

    // Functions for Hackerspace door 
    bool tryFindDoor(); // Also get challenge
    void tryOpenDoor(); // Try open door
    void tryCloseDoor(); // Try close door    

  // quasi-private, usually no need to use directly 
    Adafruit_NeoPixel antennaLED;
    int pinOledSda, pinOledSck, pinPwrHold, pinAudio, pinAudioEn,
      pinTouchLU, pinTouchLD, pinTouchRU, pinTouchRD, pinNeoPixel,
      pinExtA, pinExtB, pinBoardRGB, pinBoardTx, pinBoardRx; 
    static const int NUM_TOUCH_PINS = 4;
    touchProcessor touch[NUM_TOUCH_PINS] = {-1, -1, -1, -1}; // LU, LD, RU, RD

    // Functions for Hackerspace door 
    bool loadKey(const char *priKey, const char *pubKey, const char *name);
    bool genKey(const char *priKey = NULL); 
    void genDoorName(const char *name = NULL);  // User name for door
    bool loadKey(); // From files door_pri.txt, door_pub.txt, door_nam.txt
    void saveKey(); // To files door_pri.txt, door_pub.txt, door_nam.txt
    uint8_t door_pubkey[32];
    uint8_t door_name[32];

    bool connectWifi(const char* ssid, const char* password);
    void setupOTA();
    void tryToUpdate();

    bool OTAinProgress = false;

    char hostName[32] = "hackffm-badge\0"; // add MAC address to make it unique
    char userName[128] = "$cHackFFM$nBadge\0";

    fs::FS &filesystem = LittleFS; // default LittleFS

    uint8_t espNowRxData[256]; // buffer for ESP-NOW data
    uint8_t espNowRxDataLen = 0;
    uint8_t espNowRxMac[6]; // MAC address of sender
    int8_t  espNowRxRssi = 0; // Last Rx RSSI 
    char    lastFoundDoorName[34]; // Last found door name

    void playMP3(const char *filename);
    void playStartSound();

    bool txDisplaydata(int channel); // channel = 0: stop 
    int  txDisplayChannel = 0;

  private:
    void detectHardware();
    static const int PINS_PER_VARIANT = 15; 
    int pinVariantIndex = 0; 
    static const int pinVariants[][PINS_PER_VARIANT]; // Pin-Matrix
    static const int NUM_PIN_VARIANTS;
  
    int boardType = 0; // 0: unknown, 1: blue, 2: black (detection unreliable!)
    static const char* const boardTypeTexts[]; 

    Adafruit_NeoPixel boardLED;
    uint8_t OledAddress = 0;

    elapsedMillis lastTouchRead = 0;
    bool touchUpdated = false;
    void setPowerHold(bool on);
    void initOLED();
    Face* face_ = nullptr;

    elapsedMillis but0CurrentPressedMs = 0;
    uint32_t but0WasPressedForMs = 0;
    bool but0LastPressed = false;

    bool txCommand(const char *cmd);
    uint8_t door_prikey[32];

    uint8_t lastChallenge[8];
    elapsedMillis findDoorTimeout = 0;
    uint8_t findDoorState = 0;
    
};

extern HackFFMBadgeLib HackFFMBadge;
extern HackFFMBadgeLib& Badge;  // alias for less writing but less HackFFM advertizing :-)

#endif // HACKFFM_BADGE_LIB_H