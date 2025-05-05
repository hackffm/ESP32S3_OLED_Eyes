// hackffm_badge_lib.cpp (cmd+K + cmd+0 to collapse)
#include "hackffm_badge_lib.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiMulti.h>
#include <elapsedMillis.h>
#include <Wire.h>
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#include "esp_system.h"
#include "esp_mac.h"
#endif

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Update.h>

#include <esp_now.h>
#include <esp_log.h>
#include <Ed25519.h>

// Peter Schatzmann Libs for Audio and Tasks
#include "freertos-all.h"

#include "hal/usb_serial_jtag_ll.h"
//#include "hal/usb_phy_ll.h"

#include "LL_Lib.h"

HackFFMBadgeLib HackFFMBadge;
HackFFMBadgeLib& Badge = HackFFMBadge;
Face face_inst(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);

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

#define FIRMWARE_URL "http://192.168.4.1/hackffmbadgev1_fw.bin"  // URL zur Firmware
#define VERSION_URL  "http://192.168.4.1/hackffmbadgev1_vers.txt"  // URL zur Versionsdatei

#define ESPNOW_CHANNEL 13

/**
 * Protocol Tx:
 * Off Len
 *   0   4  Preemble "D00r"
 *   4  64  Signature
 *  68  32  Public Key
 * 100  32  Name (Cleartext UTF8, 0 filled)
 * 132   8  Challenge respone
 * 140   1  Command (might be longer than 1 if needed)   
 * 
 */
bool HackFFMBadgeLib::txCommand(const char *cmd) {
  uint8_t msg[4+64+32+32+8+8];
  int cmdlen = strlen(cmd);
  if(cmdlen > 8) cmdlen = 8;
  msg[0] = 'D'; msg[1] = '0'; msg[2] = '0'; msg[3] = 'r';
  memcpy(&msg[68], door_pubkey, 32);
  memcpy(&msg[100], door_name, 32);
  memcpy(&msg[132], lastChallenge, 8);
  memcpy(&msg[140], cmd, cmdlen+1);
  Ed25519::sign(&msg[4], door_prikey, door_pubkey, &msg[68], 32+32+8+cmdlen+1);
  esp_now_peer_info_t peer = {
    .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    .channel = ESPNOW_CHANNEL,
    .encrypt = false
  };
  ESP_ERROR_CHECK(esp_now_send(peer.peer_addr, msg, 4+64+32+32+8+cmdlen+1));
  return(true);
}

void HackFFMBadgeLib::saveKey() {
  char priKey[70];
  char pubKey[70];
  char name[33];
  memset(priKey, 0, sizeof(priKey));
  memset(pubKey, 0, sizeof(pubKey));
  memset(name, 0, sizeof(name));
  bytes2hex(door_prikey, 32, priKey);
  bytes2hex(door_pubkey, 32, pubKey);
  strlcpy(name, (char *)door_name, sizeof(name)-1);
  writeFile("/door_pri.txt", priKey);
  writeFile("/door_pub.txt", pubKey);
  writeFile("/door_nam.txt", name);
}

bool HackFFMBadgeLib::loadKey() {
  // Load key from file
  String loadPriKey = readFile("/door_pri.txt");
  String loadPubKey = readFile("/door_pub.txt");
  String loadName = readFile("/door_nam.txt");
  if(loadPriKey.length() < 64) return false;
  if(loadPubKey.length() < 64) return false;
  if(loadName.length() == 0) genDoorName();
  
  return loadKey(loadPriKey.c_str(), loadPubKey.c_str(), loadName.c_str());
}

bool HackFFMBadgeLib::loadKey(const char *priKey, const char *pubKey, const char *name) {
  size_t len = 0;
  len = hex2bytes(priKey, door_prikey, 32);
  if(len != 32) return false;
  len = hex2bytes(pubKey, door_pubkey, 32);
  if(len != 32) return false;  
  memset(door_name, 0, 32);
  strlcpy((char *)door_name, name, 31);
  uint8_t temp_pubkey[32];
  Ed25519::derivePublicKey(temp_pubkey, door_prikey);
  if(memcmp(temp_pubkey, door_pubkey, 32) != 0) return false;
  return true;
}  

bool HackFFMBadgeLib::genKey(const char *priKey) {
  if(priKey == NULL) {
    Ed25519::generatePrivateKey(door_prikey);
  } else {
    size_t len = 0;
    len = hex2bytes(priKey, door_prikey, 32);
    if(len != 32) return false;
  }
  Ed25519::derivePublicKey(door_pubkey, door_prikey);
  return true;
} 

// If no name is given, generate a name from the user name
void HackFFMBadgeLib::genDoorName(const char *name) {
  char namebuf[34];
  memset(namebuf, 0, sizeof(namebuf));
  if(name != NULL) {
    strlcpy(namebuf, name, sizeof(namebuf)-1);
  } else {
    snprintf(namebuf, sizeof(namebuf)-1, "HBdg %s", userName);
  }
  strlcpy((char *)door_name, namebuf, sizeof(door_name)-1);
} 



// Callback receive
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void on_data_recv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len) {
  const uint8_t *mac_addr  = esp_now_info->src_addr;
  #else
void on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int len) {
#endif  
  if(len > 1) {
    Badge.espNowRxMac[0] = mac_addr[0];
    Badge.espNowRxMac[1] = mac_addr[1];
    Badge.espNowRxMac[2] = mac_addr[2];
    Badge.espNowRxMac[3] = mac_addr[3];
    Badge.espNowRxMac[4] = mac_addr[4];
    Badge.espNowRxMac[5] = mac_addr[5];
    memcpy(Badge.espNowRxData, data, len);
    Badge.espNowRxDataLen = len;
  }
}

// Callback transmit
void on_data_sent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.printf("Sendestatus: %s", status == ESP_NOW_SEND_SUCCESS ? "Erfolg" : "Fehler");
}

bool HackFFMBadgeLib::tryFindDoor() {
  // Open ESPNOW with timeout and send get challenge
  if(findDoorState > 0) {
    // LL_Log.println("Find door already in progress");
    if(findDoorState ==  2) return true;
    return false;
  }
  
  // WiFi-Init
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

  // ESP-NOW-Init
  ESP_ERROR_CHECK(esp_now_init());
  esp_now_register_recv_cb(on_data_recv);
  esp_now_register_send_cb(on_data_sent);

  // Peer-Info konfigurieren (Broadcast)
  esp_now_peer_info_t peer = {
      .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
      .channel = ESPNOW_CHANNEL,
      .encrypt = false
  };
  ESP_ERROR_CHECK(esp_now_add_peer(&peer));

  wifi_phy_rate_t wifi_rate = WIFI_PHY_RATE_48M; // WIFI_PHY_RATE_MCS5_SGI;
 // wifi_rate = WIFI_PHY_RATE_LORA_250K;
 // wifi_rate = WIFI_PHY_RATE_1M_L;
  wifi_rate = WIFI_PHY_RATE_24M;
 // wifi_rate = WIFI_PHY_RATE_MCS5_LGI;
 
  esp_wifi_set_max_tx_power(28); // 7 dbm
  
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N /* |WIFI_PROTOCOL_LR */);
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_now_rate_config_t en_rateconfig = {
    .phymode = WIFI_PHY_MODE_HT20, // WIFI_PHY_MODE_11G,
    .rate = WIFI_PHY_RATE_MCS4_SGI,
    .ersu = false,   
    .dcm = false,
  };
  esp_now_set_peer_rate_config(peer.peer_addr, &en_rateconfig);
  #else
  esp_wifi_config_espnow_rate(WIFI_IF_STA,  wifi_rate);
  #endif

  txCommand("c");
  findDoorState = 1;
  findDoorTimeout = 0;

  return false;

}

void HackFFMBadgeLib::tryOpenDoor() {
  if(findDoorState == 2) {
    txCommand("O"); findDoorState++;
    findDoorTimeout = 0;
  }
}

void HackFFMBadgeLib::tryCloseDoor() {
  if(findDoorState == 2) {
    txCommand("C"); findDoorState++;
    findDoorTimeout = 0;
  }
}

void HackFFMBadgeLib::begin() {
  // Face must be created early, maybe some memory initialization issues there in... (weird eyes come out)
  //if(face_) delete face_;
  //face_ = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  face_ = &face_inst;

  // a little less sound at boot
  pinMode(5, OUTPUT); digitalWrite(5, LOW); 
  pinMode(6, OUTPUT); digitalWrite(6, LOW);
  pinMode(7, OUTPUT); digitalWrite(7, LOW);
  
  // Power on OLED
  // Conserve power (to allow CR123 battery operation)
  WiFi.mode(WIFI_OFF);
  delay(1);

  // Activate Serial but wait no longer than 1s to be ready
  Serial.begin(115200); // Baudrate does not matter as it is done via USB CDC...
  elapsedMillis serialUpTimeout = 0;
  while (!Serial && serialUpTimeout < 1000) {
    delay(1);
  }
  LL_Log.begin(115200, 2222); // Serial baud (not used), TCP Port

  // Perform hardware detection
  detectHardware();

  // Initialize filesystem
  if(!LittleFS.begin()) {
    Serial.println("filesystem Mount Failed");
    return;
  }
  listDir("/", 0);
  
  // try to load user name from file
  String loadUserName = readFile("/name.txt");
  if(loadUserName.length() > 0) strlcpy(userName, loadUserName.c_str(), sizeof(userName)-1);
  
  // try to load host name from file
  uint8_t baseMac[6];
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_read_mac(baseMac, ESP_MAC_EFUSE_FACTORY);
  #else
  esp_efuse_mac_get_default(baseMac);
  #endif
  Serial.printf(" Base MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n", 
    (unsigned int)baseMac[0], (unsigned int)baseMac[1], 
    (unsigned int)baseMac[2], (unsigned int)baseMac[3], 
    (unsigned int)baseMac[4], (unsigned int)baseMac[5]);
  delay(10);
  String loadHostName = readFile("/hostname.txt");
  if(loadHostName.length() > 0) strlcpy(hostName, loadHostName.c_str(), sizeof(hostName)-1);
  else {
    // Generate a random name
    sprintf(hostName, "hackffm-badge-%02X%02X%02X", (unsigned int)baseMac[3], (unsigned int)baseMac[4], (unsigned int)baseMac[5]);
  }
  Serial.printf(" HostName: %s\r\n", hostName);

  // Try to load key from file
  if(loadKey() == false) {
    Serial.println(" loadKey failed - autogenerate new key");
    genKey();
    saveKey();
  } 
  
  char buf[70];
  //bytes2hex(door_prikey, 32, buf);
  //Serial.printf(" Door Private key: %s\r\n", buf);
  bytes2hex(door_pubkey, 32, buf);
  Serial.printf(" Door Public key:  %s\r\n", buf);
  Serial.printf(" Door Name: %s\r\n", door_name);

  // Start face and set some default values
  //if(face_) delete face_;
  //face_ = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  faceActive = true;
  face().Expression.GoTo_Normal();
  face().Behavior.GoToEmotion(eEmotions::Normal);
  face().Behavior.SetEmotion(eEmotions::Normal, 0.8);
  face().Behavior.SetEmotion(eEmotions::Glee, 0.05);
  face().Behavior.SetEmotion(eEmotions::Worried, 0.05);
  face().Behavior.SetEmotion(eEmotions::Surprised, 0.05);
  face().Behavior.SetEmotion(eEmotions::Skeptic, 0.05);
  // Automatically switch between behaviours (selecting new behaviour randomly based on the weight assigned to each emotion)
  face().RandomBehavior = true;
  face().Behavior.Timer.SetIntervalMillis(6000); // 6s

  face().RandomBlink = true; // Automatically blink
  face().Blink.Timer.SetIntervalMillis(4000);

  // Automatically choose a new random direction to look
  //face().RandomLook = true;  


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
    lastTouchLU = (float)touch[0].getTouchValue() / 50.0;
    lastTouchLD = (float)touch[1].getTouchValue() / 50.0;
    lastTouchRU = (float)touch[2].getTouchValue() / 50.0;
    lastTouchRD = (float)touch[3].getTouchValue() / 50.0;
    lastTouchX = (-lastTouchLU) + (-lastTouchLD) + (lastTouchRU) + (lastTouchRD);
    lastTouchY = (-lastTouchLU) + (-lastTouchRU) + (lastTouchLD) + (lastTouchRD);
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

  if(findDoorState > 0) {
    if(findDoorTimeout > 4000) {
      findDoorState = 0;
      esp_now_deinit();
      WiFi.mode(WIFI_OFF);
      Serial.println("Find door timeout");
    } else {
      if(findDoorState < 10) {
        if(espNowRxDataLen > 0) {
          Serial.printf("RxLen: %d\r\n", espNowRxDataLen);
          // Check if it is valid Door protocol
          if(espNowRxDataLen >= (4+32+8+1)) {
            if(((espNowRxData[0] == 'D') && (espNowRxData[1] == '0') && (espNowRxData[2] == '0') && (espNowRxData[3] == 'a'))) {
              if(memcmp(&espNowRxData[4], door_pubkey, 32) == 0) {
                char cmd = espNowRxData[44];
                memcpy(lastChallenge, &espNowRxData[36], 8);
        
                switch (findDoorState)
                {
                  case 1: // challenge requested, expect now 'c'
                    if(cmd == 'c') findDoorState++; else findDoorState = 100;
                    //txCommand("t");
                    break;
        
                  case 3: // trigger requested, expect now 't'
                    if(cmd == 't') findDoorState++; else findDoorState = 100;
                    break;
                  
                  default:
                    break;
                }
                Serial.printf("RxCmd: %.*s\n", espNowRxDataLen-44, &espNowRxData[44]);
              }  
            }
          }
          
          espNowRxDataLen = 0;
        }
      }
    }
  }
    
}

void HackFFMBadgeLib::listDir(const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  fs::File root = filesystem.open(dirname);
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
  File file = filesystem.open(path, "w", true); // "w" overwrite file
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
  String data = "";
  if(filesystem.exists(path) == false) {
    // LL_Log.printf("File %s does not exist\r\n", path);
    return data;
  }
  File file = filesystem.open(path, "r");  
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

void HackFFMBadgeLib::drawLog(bool noDraw) {
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
  if(!noDraw) u8g2.sendBuffer();  
  u8g2.setDrawColor(1);
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

void HackFFMBadgeLib::drawBMP(const char *filename, bool noDraw) {
  File file = filesystem.open(filename, "r");
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

  //u8g2.clearBuffer(); 

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

  if(!noDraw) u8g2.sendBuffer();  
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
  char buf[512];
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
          case 'g': u8g2.setFont(u8g2_font_inr21_mf); break; // ?
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
  String BootMsg = "$!$c$6HackFFM Badge V1\n";
  BootMsg += String(__DATE__) + "\n" + String(__TIME__);

  drawString(BootMsg.c_str());

  u8g2log.begin(U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);	// assign only buffer, update full manual
  u8g2log.print("u8g2log:\r\n");
}

void HackFFMBadgeLib::detectHardware() {
  pinVariantIndex = 0; OledAddress = 0;

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
    boardLED.setPin(48);
  } else if(pina3 > 3200) {
    boardType = 1; // blue
    boardLED.setPin(21);
  } else {
    boardType = 0; // unknown
  }
  Serial.printf("BoardType: %s\n", boardTypeTexts[boardType]);

  boardLED.begin();
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
  if(pinBoardRGB >= 0) {
    boardLED.setPin(pinBoardRGB);
    boardLED.begin();
  }
  if(pinNeoPixel >= 0) {
    antennaLED.setPin(pinNeoPixel);
    antennaLED.begin();
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
    antennaLED.fill(12, 4, 0); // orange
    setAntennaLED(12, 4, 0); 
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
  Serial.printf(" ESP32 Arduino version: %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH); 
  

  // Init touch processors
  touch[0] = touchProcessor(pinTouchLU);
  touch[1] = touchProcessor(pinTouchLD);
  touch[2] = touchProcessor(pinTouchRU);
  touch[3] = touchProcessor(pinTouchRD);

  setPowerAudio(false);
}

void HackFFMBadgeLib::playStartSound() {
  // play tone duh-duett
  setPowerAudio(true);

  /*
  delay(100);
  writeTone(784); // G5
  delay(300);
  writeTone(1047); // C6
  delay(300);
  */
  

  delay(500);
  writeTone(660);

  
  delay(100);
  writeTone(0);
  delay(150);

  writeTone(660);
  delay(100);
  writeTone(0);
  delay(300);

  writeTone(660);
  delay(100);
  writeTone(0);
  delay(300);

  writeTone(510);
  delay(100);
  writeTone(0);
  delay(100);

  writeTone(660);
  delay(100);
  writeTone(0);
  delay(300);

  writeTone(770);
  delay(100);
  writeTone(0);
  delay(550);

  writeTone(380);
  delay(100);
  writeTone(0);
   
  delay(575);

 

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
  //if(freenoveBoardLED) {
  //  freenoveBoardLED->setLedColor(0, r, g, b);
 // } 
  boardLED.setPixelColor(0, r, g, b);
  boardLED.show();  
}

void HackFFMBadgeLib::setAntennaLED(uint32_t rgb) {
  setAntennaLED(rgb >> 16, rgb >> 8, rgb);
}

void HackFFMBadgeLib::setAntennaLED(uint8_t r, uint8_t g, uint8_t b) {
  //if(freenoveAntennaLED) {
  //  freenoveAntennaLED->setLedColor(0, r, g, b);
  //} 
  antennaLED.setPixelColor(0, r, g, b);
  antennaLED.show();
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

bool HackFFMBadgeLib::connectWifi(const char* ssid, const char* password) {
  bool ret = false;
  WiFi.setHostname(hostName);
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
    setupOTA();
    ret = true;
  } else {
    LL_Log.println("Connection failed.");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
  }
  return ret;
}

void HackFFMBadgeLib::setupOTA() {
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
  
  MDNS.begin(hostName);
  MDNS.addService("debug", "tcp", 2222);
}

void HackFFMBadgeLib::tryToUpdate() {
  WiFi.disconnect();
  if (connectWifi("HACKFFM_BADGE_UPDATER", "")) {
    LL_Log.println("Connected to HACKFFM_BADGE_UPDATER");
    LL_Log.println("Starting OTA update...");
    HTTPClient http;
    http.begin(VERSION_URL);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String version = http.getString();
        LL_Log.println("Firmware version on server: " + version);
    }
    http.end();
    http.begin(FIRMWARE_URL);
    httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        if (contentLength > 0) {
            drawString("$!$c$8$+$+$+$+$+$+$+Updating FW...");
            WiFiClient * stream = http.getStreamPtr();
            if (Update.begin(contentLength)) {
                size_t written = Update.writeStream(*stream);
                if (written == contentLength && Update.end()) {
                    drawString("$nDone."); delay(3000);
                    ESP.restart();
                }
            }
        }
    }
    http.end();
    LL_Log.println("OTA update finished.");
  } else {
    LL_Log.println("Failed to connect to HACKFFM_BADGE_UPDATER");
  }
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

uint8_t hex1(const char c) {
  uint8_t val = 255;
  if((c >= 'A') && (c <= 'F')) val = c - 'A' + 10;
  if((c >= 'a') && (c <= 'f')) val = c - 'a' + 10;
  if((c >= '0') && (c <= '9')) val = c - '0';
  return val;
}

size_t hex2bytes(const char *hexString, uint8_t *byteArray, size_t byteArrayLen) {
  size_t byteCount = 0;

  while(byteCount < byteArrayLen) {
    uint8_t h = 0;
    uint8_t l = 0;
    char c;
    while((h = hex1(c = *hexString++)) == 255) {
      if(c == 0) break;
    }
    if(c == 0) break;
    while((l = hex1(c = *hexString++)) == 255) {
      if(c == 0) break;
    }
    if(c == 0) break;    
    *byteArray++ = h*16+l;
    byteCount++;
  }

  return byteCount;
}

void bytes2hex(const uint8_t *byteArray, size_t byteArrayLen, char *hexString) {
  for(size_t i = 0; i < byteArrayLen; i++) {
    sprintf(hexString + (i*2), "%02X", byteArray[i]);
  }
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

bool is_host_usb_device_connected(void)
{
	return *((volatile uint32_t *)USB_SERIAL_JTAG_OUT_EP0_ST_REG) != 0;
}

void disconnect_cdc_port(void)
{
	*((volatile uint32_t *)USB_SERIAL_JTAG_CONF0_REG) &= ~(1<<9);
}

void connect_cdc_port(void)
{
	*((volatile uint32_t *)USB_SERIAL_JTAG_CONF0_REG) |= (1<<9);
}