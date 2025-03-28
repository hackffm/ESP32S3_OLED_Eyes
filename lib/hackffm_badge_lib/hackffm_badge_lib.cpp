// hackffm_badge_lib.cpp
#include "hackffm_badge_lib.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiMulti.h>
#include <elapsedMillis.h>
#include <Wire.h>
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "LL_Lib.h"

HackFFMBadgeLib HackFFMBadge;
HackFFMBadgeLib& Badge = HackFFMBadge;

// Displays are selected in platformio.ini
//#define U8G2_DISPLAYTYPE U8G2_SSD1306_128X64_ADAFRUIT_F_HW_I2C  /* 0.96 inch */
//#define U8G2_DISPLAYTYPE U8G2_SSD1306_128X64_NONAME_F_HW_I2C  /* 0.96 inch */
//#define U8G2_DISPLAYTYPE U8G2_SH1106_128X64_NONAME_F_HW_I2C   /* 1.3 inch */
//#define U8G2_DISPLAYTYPE U8G2_SSD1309_128X64_NONAME0_F_HW_I2C /* 2.42 inch */
// 
U8G2_DISPLAYTYPE u8g2(U8G2_R0,  /* reset=*/ U8X8_PIN_NONE);
U8G2LOG u8g2log;
#define U8LOG_FONT u8g2_font_tom_thumb_4x6_tf
#define U8LOG_FONT_X  4
#define U8LOG_FONT_Y  6
#define U8LOG_WIDTH ((U8G2_DISP_WIDTH - 4) / U8LOG_FONT_X)
#define U8LOG_HEIGHT 6
uint8_t u8log_buffer[U8LOG_WIDTH*U8LOG_HEIGHT];

// Use these for Audio output
#define LEDC_CHANNEL_AUDIO 0
#define LEDC_RESOLUTION_AUDIO 8

void HackFFMBadgeLib::begin() {
  // Conserve power (to allow CR123 battery operation)
  WiFi.mode(WIFI_OFF);
  delay(1);

  // Activate Serial but wait no longer than 1s to be ready
  Serial.begin(115200); // Baudrate does not matter as it is done via USB CDC...
  elapsedMillis serialUpTimeout = 0;
  while (!Serial && serialUpTimeout < 1000) {
    delay(1);
  }

  // Perform hardware detection
  detectHardware();

  // Initialize LittleFS
  if(!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  listDir("/", 0);

  // Start face
  if(face_) delete face_;
  face_ = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  faceActive = true;

  delay(3000);
  Serial.println("HackFFM Badge initialized");
}

// Touch sensor update, ArduinoOTA and Face update
void HackFFMBadgeLib::update() {
  if(lastTouchRead > 100) {
    lastTouchRead = 0;
    for(int i = 0; i < NUM_TOUCH_PINS; i++) {
      touch[i].update();
    }
    touchUpdated = true;
  }

  // Update face without drawing
  if((faceActive) && (face_)) {
    face_->Update(true);
  }

  ArduinoOTA.handle();
  if(OTAinProgress == true) {
    for(int i=0; i<20; i++) {
      delay(10);
      ArduinoOTA.handle();
    }
  } 
    
}

void HackFFMBadgeLib::listDir(const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  fs::File root = LittleFS.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  fs::File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }

  size_t t = LittleFS.totalBytes();
  size_t u = LittleFS.usedBytes();
  Serial.printf("Total space: %d, used space: %d, free space: %d\r\n", t, u, t - u);
}

// Function to write a string to a file
bool HackFFMBadgeLib::writeFile(const char *path, const String &data) {
  File file = LittleFS.open(path, "w"); // "w" overwrite file
  if (!file) {
    LL_Log.println("Can't open file to write");
      return false;
  }
  file.print(data);
  file.close();
  LL_Log.println("File written");
  return true;
}

// Function to read a file and return the content as a string
String HackFFMBadgeLib::readFile(const char *path) {
  File file = LittleFS.open(path, "r");  
  String data = "";
  if (!file) {
    LL_Log.println("Can't open file for reading");
      return data;
  }
  while (file.available()) {
      data += (char)file.read();
  }
  file.close();
  return data;
}

void HackFFMBadgeLib::writeTone(uint32_t freq /* 0 to stop */, uint32_t duty) {
  if(pinAudio < 0) return;
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    if(freq == 0) {
      ledcDetach(pinAudio);
    } else {
      ledcAttachChannel(pinAudio, freq, LEDC_RESOLUTION_AUDIO, LEDC_CHANNEL_AUDIO);
      ledcWrite(pinAudio, duty);
    }
  #else
    if(freq == 0) {
      ledcDetachPin(pinAudio);
    } else {
      ledcSetup(LEDC_CHANNEL_AUDIO, freq, LEDC_RESOLUTION_AUDIO);
      ledcAttachPin(pinAudio, LEDC_CHANNEL_AUDIO);
      ledcWrite(LEDC_CHANNEL_AUDIO, duty);
    }
  #endif
}

void HackFFMBadgeLib::drawLog() {
  // Align Log-box from lower-left corner
  int logbox_y = u8g2.getDisplayHeight() - (U8LOG_HEIGHT * U8LOG_FONT_Y) - 3;
  u8g2.setFont(U8LOG_FONT);
  u8g2.setFontPosTop();
  u8g2.setFontMode(0);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0,logbox_y,u8g2.getDisplayWidth(),u8g2.getDisplayHeight()-logbox_y);
  u8g2.setDrawColor(0);
  u8g2.drawBox(1,logbox_y+1,u8g2.getDisplayWidth()-2,u8g2.getDisplayHeight()-(logbox_y+2));
  u8g2.setDrawColor(1);
  u8g2.drawLog(2,logbox_y+1,u8g2log);
  u8g2.sendBuffer();
}

// BMP-Header-Struktur
struct __attribute__((packed)) BMPHeader {
  uint16_t signature;
  uint32_t fileSize;
  uint32_t reserved;
  uint32_t dataOffset;
  uint32_t headerSize;
  int32_t width;
  int32_t height;
  uint16_t planes;
  uint16_t bitsPerPixel;
};

void HackFFMBadgeLib::drawBMP(const char *filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
      Serial.println("Can't open file!");
      return;
  }

  BMPHeader header;
  file.read((uint8_t *)&header, sizeof(BMPHeader));

  /*
  Serial.printf("Signature: %x\r\n", header.signature);
  Serial.printf("Filesize: %d\r\n", header.fileSize);   
  Serial.printf("DataOffset: %d\r\n", header.dataOffset);
  Serial.printf("HeaderSize: %d\r\n", header.headerSize);
  Serial.printf("Width: %d\r\n", header.width); 
  Serial.printf("Height: %d\r\n", header.height);
  Serial.printf("Planes: %d\r\n", header.planes);
  Serial.printf("BitsPerPixel: %d\r\n", header.bitsPerPixel);
  */

  if (header.signature != 0x4D42 || header.bitsPerPixel != 1) {
      Serial.println("BMP-Format not supported!");
      file.close();
      return;
  }

  int width = header.width;
  int height = abs(header.height);
  bool flip = (header.height > 0);
  int rowSize = ((width + 31) / 32) * 4;
  uint8_t rowBuffer[rowSize];

  u8g2.clearBuffer(); 

  for (int y = 0; y < height; y++) {
      int rowIndex = flip ? (height - 1 - y) : y;
      file.seek(header.dataOffset + rowIndex * rowSize, SeekSet);
      file.read(rowBuffer, rowSize);

      for (int x = 0; x < width; x++) {
          int byteIndex = x / 8;
          int bitIndex = 7 - (x % 8);
          bool pixel = (rowBuffer[byteIndex] >> bitIndex) & 1;
          u8g2.setDrawColor(pixel ? 1 : 0);
          u8g2.drawPixel(x, y);
      }
  }
  file.close();

  u8g2.sendBuffer();  
  u8g2.setDrawColor(1);
}

/* Special characters:
 * 0x0a \n: new line
 * $: Escape, next char defines action
 *    $: prints $
 *    c,l,r: center/left/right aligned
 *    0..9,a,b,d,e,f,g: set fonts
 *    !: clear display
 */
void HackFFMBadgeLib::drawString(const char *str, int x, int y, int dy, bool noDraw) {
  if(!OledAddress) return;
  char buf[127];
  char c;
  int  idx = 0;
  int w = u8g2.getDisplayWidth();
  int sw = 0; // getUTF8Width(
  int dsFlags = 0;
  int newX = x;

   do {
    c = *str++;
    if(c>=' ') {
      if(dsFlags & 1) {
        dsFlags &= ~(1);
        switch(c) {
          case '$': buf[idx++] = c; break;
          case 'n': c = '\n'; goto LINEFEED; break;
          case '!': u8g2.clearBuffer(); break;
          case '+': y++; break;
          case '-': y--; break;
          case 'c':
            dsFlags &= ~(6);
            dsFlags |= 2;
            break;
          case 'l':
            dsFlags &= ~(6);
            break; 
          case 'r':
            dsFlags &= ~(6);
            dsFlags |= 4;
            break;
          // Fonts
          case '0': u8g2.setFont(u8g2_font_tinyunicode_tf); break; // 27  
          case '1': u8g2.setFont(u8g2_font_nokiafc22_tf); break; // 22.2  
          case '2': u8g2.setFont(u8g2_font_busdisplay11x5_te); break;   // 21.5
          case '3': u8g2.setFont(u8g2_font_sonicmania_te); break; // 19.1          nice bold
          case 'E': u8g2.setFont(u8g2_font_prospero_bold_nbp_tf); break; // ?          nice bold
          case '4': u8g2.setFont(u8g2_font_DigitalDiscoThin_tf); break; // 16.2
          case '5': u8g2.setFont(u8g2_font_scrum_tf); break; // 16 a
          case '6': u8g2.setFont(u8g2_font_DigitalDisco_tf); break; // 16 b        nice bold
          case 'G': u8g2.setFont(u8g2_font_profont17_tf); break; // 16 b        nice bold
          case '7': u8g2.setFont(u8g2_font_fur11_tf); break; // 16 c
          case '8': u8g2.setFont(u8g2_font_Pixellari_tf); break; // 15.5           nice bold
          case '9': u8g2.setFont(u8g2_font_fub11_tf); break; // 14.1
          case 'a': u8g2.setFont(u8g2_font_lucasarts_scumm_subtitle_o_tf); break; // 12
          case 'b': u8g2.setFont(u8g2_font_lucasarts_scumm_subtitle_r_tf); break; // 12          
          case 'd': u8g2.setFont(u8g2_font_fur14_tf); break; // 11.5
          case 'e': u8g2.setFont(u8g2_font_fub14_tf); break; // 11.5
          case 'f': u8g2.setFont(u8g2_font_chargen_92_tf); break; //10.9
          case 's': u8g2.setFont(u8g2_font_sticker100complete_te); break; // ?
          case 'i': u8g2.setFont(u8g2_font_m2icon_9_tf); break;
                             
        }
      } else {
        if(c=='$') {
          dsFlags |= 1;
        } else {
          buf[idx++] = c;
        }        
      }
    } else {
      LINEFEED:
      switch(c) {
        case 0:
        case '\n':
          buf[idx] = 0;
          sw = u8g2.getUTF8Width(buf);
          if(dsFlags & 2) {
            // centered (plus offsetted by x)
            newX = (w - sw)/2 + x;
          } else if(dsFlags & 4) {
            // right (minus offsetted by x)
            newX = (w - sw) - x;
          } else {
            newX = x;
          }
          y += u8g2.getAscent()-u8g2.getDescent()+dy;
          u8g2.drawUTF8(newX,y,buf);
          idx = 0;
          break; 
      }
    }
  } while(c);

  if(!noDraw) u8g2.sendBuffer();

}

void HackFFMBadgeLib::initOLED() {
  // Unlike almost every other Arduino library (and the I2C address scanner script etc.)
  // u8g2 uses 8-bit I2C address, so we shift the 7-bit address left by one
  u8g2.setI2CAddress(OledAddress<<1);
  u8g2.begin();
  u8g2.enableUTF8Print();

  // make it faster
  u8g2.setBusClock(1000000UL);

  // dim display down 
  //u8g2.sendF("ca", 0xdb, 0); // not for SH1106 - need af (e3) afterwards
  //u8g2.sendF("ca", 0xd9, 0x2f);
  //u8g2.sendF("ca", 0xaf); // reactivate if display turns off (SH1106 need it)
  //u8g2.setContrast(0x32); // u8g2.sendF("ca", 0x81, 32);

  drawString("$!$c$f$+$+$+HackFFM\n$+Badge V1");

  u8g2log.begin(U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);	// assign only buffer, update full manual
  u8g2log.print("u8g2log:\r\n");
}

void HackFFMBadgeLib::detectHardware() {
  pinVariantIndex = 0; OledAddress = 0;
  if(freenoveBoardLED) { delete freenoveBoardLED; freenoveBoardLED = nullptr; }
  if(freenoveAntennaLED) { delete freenoveAntennaLED; freenoveAntennaLED = nullptr; }

  // Try to detect blue or black board by checking GPIO3
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  delay(2);
  pinMode(3, INPUT_PULLUP);
  delay(2);
  int pina3 = analogRead(3);  // black: 2933, 2955  blue: 3685
  Serial.printf("GPIO3 ADC: %d\n", pina3);
  if((pina3 > 2700) && (pina3 < 3200)) {
    boardType = 2; // black
    freenoveBoardLED = new Freenove_ESP32_WS2812(1, 48, 0);
    freenoveBoardLED->begin();
  } else if(pina3 > 3200) {
    boardType = 1; // blue
    freenoveBoardLED = new Freenove_ESP32_WS2812(1, 21, 0);
    freenoveBoardLED->begin();
  } else {
    boardType = 0; // unknown
  }
  Serial.printf("BoardType: %s\n", boardTypeTexts[boardType]);

  setBoardLED(0, 1, 0); // green dark

  // Try find OLED display which defines board variant
  int ret = 0;  // 0: not found
  for(int i = 1; i < NUM_PIN_VARIANTS; i++) {
    TwoWire WireTemp = TwoWire(0);
    //WireTemp.endTransmission();
    WireTemp.begin(pinVariants[i][0], pinVariants[i][1], 100000);
    WireTemp.beginTransmission(0x3c);
    if(WireTemp.endTransmission() == 0) {
      WireTemp.end();
      OledAddress = 0x3c;
      ret = i;
      break;
    }
    WireTemp.beginTransmission(0x3d);
    if(WireTemp.endTransmission() == 0) {
      WireTemp.end();
      OledAddress = 0x3d;
      ret = i;
      break;
    }
    WireTemp.end();
  }

  // Set pins according to detected variant
  pinVariantIndex = ret;
  pinOledSda  = pinVariants[pinVariantIndex][0];
  pinOledSck  = pinVariants[pinVariantIndex][1];
  pinPwrHold  = pinVariants[pinVariantIndex][2];
  pinAudio    = pinVariants[pinVariantIndex][3];
  pinAudioEn  = pinVariants[pinVariantIndex][4];
  pinTouchLU  = pinVariants[pinVariantIndex][5];
  pinTouchLD  = pinVariants[pinVariantIndex][6];
  pinTouchRU  = pinVariants[pinVariantIndex][7];
  pinTouchRD  = pinVariants[pinVariantIndex][8];
  pinNeoPixel = pinVariants[pinVariantIndex][9];
  pinExtA     = pinVariants[pinVariantIndex][10];
  pinExtB     = pinVariants[pinVariantIndex][11];
  pinBoardRGB = pinVariants[pinVariantIndex][12];
  pinBoardTx  = pinVariants[pinVariantIndex][13];
  pinBoardRx  = pinVariants[pinVariantIndex][14];

  // Set pins
  pinMode(0, INPUT_PULLUP); // Button
  pinMode(pinBoardRx, INPUT_PULLUP);

  // Set power hold active
  setPowerHold(true);

  // Re-Init NeoPixel
  if(freenoveBoardLED) { delete freenoveBoardLED; freenoveBoardLED = nullptr; }
  if(freenoveAntennaLED) { delete freenoveAntennaLED; freenoveAntennaLED = nullptr; }
  if(pinBoardRGB >= 0) {
    freenoveBoardLED = new Freenove_ESP32_WS2812(1, pinBoardRGB, 0);
    freenoveBoardLED->begin();
  }
  if(pinNeoPixel >= 0) {
    freenoveAntennaLED = new Freenove_ESP32_WS2812(10, pinNeoPixel, 1, TYPE_GRB);
    freenoveAntennaLED->begin();
  }

  // Init OLED display if found or blink red-blue 5 times
  if(ret >= 1) {
    Wire.begin(pinOledSda, pinOledSck, 400000);
    initOLED();
    Serial.printf("OLED Display found at 0x%02x on SDA=%d, SCK=%d\n", OledAddress, pinOledSda, pinOledSck);
    switch(pinVariantIndex) {
      case 1: Serial.println("Badge PCB V2, blue  ESP32-S3-Zero"); break;
      case 2: Serial.println("Badge PCB V2, black ESP32-S3 Super Mini"); break;
      default: Serial.println("Unknown variant"); break;
    }
    setBoardLED(0, 1, 0); // green dark
    setAntennaLED(12, 4, 0); // orange
  } else {
    Serial.println("No OLED display found - detection of variant not possible, other hardware not available!");

    // blink red-blue 5 times
    for(int i = 0; i < 5; i++) {
      setBoardLED(4, 0, 0); 
      setAntennaLED(4, 0, 0);
      delay(500);
      setBoardLED(0, 0, 4); 
      setAntennaLED(0, 0, 4);
      delay(500);
    }
    // stay red dark
    setBoardLED(1, 0, 0);
    setAntennaLED(1, 0, 0);
    
  }

  // Output some HW-Informations
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  Serial.printf(" ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf(" This chip has %d cores, Chip ID: %08x\n", ESP.getChipCores(), chipId);
  Serial.printf(" Silicon revision: %d\n", ESP.getChipRevision());
  Serial.printf(" CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf(" Free heap RAM: %u Bytes\n", (uint32_t)ESP.getFreeHeap());
  Serial.printf(" Total flash: %u Bytes\n", (uint32_t)ESP.getFlashChipSize());
  Serial.printf(" Flash for main program: %u Bytes\n", (uint32_t)ESP.getFreeSketchSpace());
  Serial.printf(" PSRAM size: %u Bytes\n", (uint32_t)ESP.getPsramSize());
  Serial.printf(" SDK version: %s\n", ESP.getSdkVersion());

  // Init touch processors
  touch[0] = touchProcessor(pinTouchLU);
  touch[1] = touchProcessor(pinTouchLD);
  touch[2] = touchProcessor(pinTouchRU);
  touch[3] = touchProcessor(pinTouchRD);

  // play tone duh-duett
  setPowerAudio(true);
  delay(100);
  writeTone(784); // G5
  delay(300);
  writeTone(1047); // C6
  delay(300);
  writeTone(0); // stop tone
  setPowerAudio(false);

}

const int HackFFMBadgeLib::pinVariants[][HackFFMBadgeLib::PINS_PER_VARIANT] = {
/**
 * Supported MCU boards:
 *  https://www.waveshare.com/wiki/ESP32-S3-Zero  
 *    PCB color blue, 4MB Flash, 2MB PSRAM
 *    GPIO21 RGB-LED, GPIO43 TX, GPIO44 RX, GPIO0 BOOT-Button 
 *    LDO has 100mV drop at 300mA -> Connect battery to 5V input should be feasible
 * 
 *  https://forum.fritzing.org/t/part-request-esp32-s3-supermini/22633 Tenstar Robot
 *    PCB color black, 4MB Flash, 2MB PSRAM
 *    GPIO48 RGB-LED + LED red, GPIO43 TX, GPIO44 RX, GPIO0 BOOT-Button
 *    GPIO3 has some weird resistor divider to 2.5V that breaks the touch sensor
 *    This board has an interal battery charger that blinks a blue LED also when no battery is connected
 * 
 */
  //  OledSda         AudioEn         TouchRD C       BoardRGB
  //      OledSck         TouchLU B       NeoPixel        BoardTx
  //          PwrHold         TouchLD A       ExtA            BoardRx
  //              Audio           TouchRU D       ExtB
    { -1, -1, -1, -1, -1, -1, -1, -1, -1, 48, -1, -1, 21, 43, 44},  // unknown, try to activate RGB-LEDs
    { 8,  7,  10, 6,  5,  4,  1,  11, 12, 9,  2,  3,  21, 43, 44},  // Badge PCB V2, blue  ESP32-S3-Zero
    { 9,  8,  11, 7,  6,  5,  2,  12, 13, 10, 3,  4,  48, 43, 44},  // Badge PCB V2, black ESP32-S3 Super Mini
};

const int HackFFMBadgeLib::NUM_PIN_VARIANTS = sizeof(HackFFMBadgeLib::pinVariants) / sizeof(HackFFMBadgeLib::pinVariants[0]);

const char* const HackFFMBadgeLib::boardTypeTexts[] = {"unknown", "blue", "black"};

void HackFFMBadgeLib::setBoardLED(uint32_t rgb) {
  setBoardLED(rgb >> 16, rgb >> 8, rgb);
}

void HackFFMBadgeLib::setBoardLED(uint8_t r, uint8_t g, uint8_t b) {
  if(freenoveBoardLED) {
    freenoveBoardLED->setLedColor(0, r, g, b);
  } 
}

void HackFFMBadgeLib::setAntennaLED(uint32_t rgb) {
  setAntennaLED(rgb >> 16, rgb >> 8, rgb);
}

void HackFFMBadgeLib::setAntennaLED(uint8_t r, uint8_t g, uint8_t b) {
  if(freenoveAntennaLED) {
    freenoveAntennaLED->setLedColor(0, r, g, b);
  } 
}

void HackFFMBadgeLib::powerOff() {  
  // Power off the badge
  // Turn off the RGB-LEDs
  setBoardLED(0, 0, 0);
  setAntennaLED(0, 0, 0);

  // Turn off the OLED display
  u8g2.setPowerSave(1);

  // Turn off the audio amplifier
  setPowerAudio(false);

  // Turn off the battery
  setPowerHold(false);

  delay(1000);

  // Turn off the ESP32-S3
  esp_deep_sleep_start();

}

void HackFFMBadgeLib::setPowerHold(bool on) {
  if(pinPwrHold >= 0) {
    pinMode(pinPwrHold, OUTPUT);
    digitalWrite(pinPwrHold, on?HIGH:LOW);
  }
}

void HackFFMBadgeLib::setPowerAudio(bool on) {
  if(pinAudioEn >= 0) {
    pinMode(pinAudioEn, OUTPUT);
    digitalWrite(pinAudioEn, on?LOW:HIGH);
  }
}

uint32_t HackFFMBadgeLib::but0PressedSince() {
  uint32_t  ret = 0;
  if(digitalRead(0) == LOW) {
    if(but0LastPressed == false) {
      // is pressed, was not pressed before
      but0LastPressed = true;
      but0CurrentPressedMs = 0;
      ret = 0; // We don't know how long it was pressed before...
    } else {  
      ret = (uint32_t)but0CurrentPressedMs;
    }
  } else {
    if(but0LastPressed == false) {
      // is not pressed and was not pressed before
      ret = 0;
    } else {
      // is not pressed, but was pressed before
      ret = 0; // as it is not pressed any longer
      but0LastPressed = false;
      but0WasPressedForMs = (uint32_t)but0CurrentPressedMs;
      but0CurrentPressedMs = 0;
    } 
  }
  return ret;
}

uint32_t HackFFMBadgeLib::but0PressedFor() {
  but0PressedSince(); // call to update but0WasPressedForMs
  uint32_t ret = but0WasPressedForMs;
  but0WasPressedForMs = 0;
  return ret;
}

void HackFFMBadgeLib::connectWifi(const char* ssid, const char* password, const char* hostname) {
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_7dBm);
  WiFi.setSleep(true);
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

void HackFFMBadgeLib::setupOTA(const char* hostname) {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    LL_Log.println("Start updating " + type);
    Badge.OTAinProgress = true;
  });
  ArduinoOTA.onEnd([]() {
    LL_Log.println("\r\nOTA Done.");
    Badge.OTAinProgress = false;
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
