/**
   The MIT License (MIT)
*/

#include <MyCreds.h>      // Define WIFI_SSID and WIFI_PASSWORD here
#include <elapsedMillis.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>

#include "LL_Lib.h"

// Activate none if no display is needed
//#define U8G2_DISPLAYTYPE U8G2_SSD1306_128X64_ADAFRUIT_F_HW_I2C  /* 0.96 inch */
//#define U8G2_DISPLAYTYPE U8G2_SSD1306_128X64_NONAME_F_HW_I2C  /* 0.96 inch */
//#define U8G2_DISPLAYTYPE U8G2_SH1106_128X64_NONAME_F_HW_I2C   /* 1.3 inch */
//#define U8G2_DISPLAYTYPE U8G2_SSD1309_128X64_NONAME0_F_HW_I2C /* 2.42 inch */

#include <U8g2lib.h>
#include <Wire.h>
//extern U8G2_DISPLAYTYPE u8g2; 
U8G2_DISPLAYTYPE u8g2(U8G2_R2,  /* reset=*/ U8X8_PIN_NONE);
U8G2LOG u8g2log;

#include "Face.h"
Face *face;
const eEmotions moods[] = {eEmotions::Angry, eEmotions::Sad, eEmotions::Surprised}; 


#define U8G2_DISP_WIDTH  128 /* 128 */
#define U8G2_DISP_HEIGHT 64
#define U8LOG_FONT u8g2_font_tom_thumb_4x6_tf
#define U8LOG_FONT_X  4
#define U8LOG_FONT_Y  6
#define U8LOG_WIDTH ((U8G2_DISP_WIDTH - 4) / U8LOG_FONT_X)
#define U8LOG_HEIGHT 6

uint8_t u8log_buffer[U8LOG_WIDTH*U8LOG_HEIGHT];

const char* hostname = "esp32-oled";
// SSD1306Wire display(0x3c, 5, 4);   
LL_Led LedBlue(16, true);
bool OTAinProgress = false;

void setup() {
  LL_Log.begin(115200, 2222); // Serial baud, TCP Port
  LL_Log.println("Booted.");

  Wire.begin(5,4);
  Wire.setClock(400000);

    LL_Log.println("OLED Display found.");

    u8g2.begin();
    u8g2.enableUTF8Print();
    
    // dim display down 
    //u8g2.sendF("ca", 0xdb, 0); // not for SH1106 - need af (e3) afterwards
    //u8g2.sendF("ca", 0xd9, 0x2f);
    
    u8g2.sendF("ca", 0xaf); // reactivate if display turns off (SH1106 need it)
    u8g2.setContrast(0x32); // u8g2.sendF("ca", 0x81, 32);
    
    u8g2.clearBuffer();          // clear the internal memory
    u8g2.setFont(u8g2_font_fub14_tf); // choose a suitable font
    u8g2.setCursor(0, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0,0,127,63);
    u8g2.setDrawColor(0);
    u8g2.print("LL_Lib V1.0");  // write something to the internal memory
    u8g2.sendBuffer();

    u8g2log.begin(U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);	// assign only buffer, update full manual


  u8g2log.print("Wifi connecting...\r\n");

  LL_Log.println("\r\nStart-up...");
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // Try to connect to a given WiFi network
  int tocount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  LL_Log.print("IP address: ");
  LL_Log.println(WiFi.localIP());

  //display.drawString(0,0,WiFi.localIP().toString().c_str());

  MDNS.begin(hostname);
  MDNS.addService("debug", "tcp", 2222);

  // Arduino OTA preparation
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

  // Create a new face
  face = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  // Assign the current expression
  face->Expression.GoTo_Normal();

  // Assign a weight to each emotion
  //face->Behavior.SetEmotion(eEmotions::Normal, 1.0);
  //face->Behavior.SetEmotion(eEmotions::Angry, 1.0);
  //face->Behavior.SetEmotion(eEmotions::Sad, 1.0);
  // Automatically switch between behaviours (selecting new behaviour randomly based on the weight assigned to each emotion)
  face->RandomBehavior = true;

  // Automatically blink
  face->RandomBlink = true;
  // Set blink rate
  face->Blink.Timer.SetIntervalMillis(4000);

  // Automatically choose a new random direction to look
  face->RandomLook = false;
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

elapsedMillis blinkTimer = 0;
void loop() {
  LL_Log.update(); LedBlue.update();

  if(blinkTimer > 100) {
    blinkTimer = 0;
    LedBlue.set(1, 1);
    if(digitalRead(0)==0) {
      LL_Log.println("Push");
    }
    int logbox_y = u8g2.getDisplayHeight() - (U8LOG_HEIGHT * U8LOG_FONT_Y) - 3;
    /*
    u8g2.setFont(U8LOG_FONT);
    u8g2.setFontPosTop();
    u8g2.setFontMode(0);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0,logbox_y,u8g2.getDisplayWidth(),u8g2.getDisplayHeight()-logbox_y);
    u8g2.setDrawColor(0);
    u8g2.drawBox(1,logbox_y+1,u8g2.getDisplayWidth()-2,u8g2.getDisplayHeight()-(logbox_y+2));
    //u8g2.drawHLine(stillAlive % u8g2.getDisplayWidth(),logbox_y,4); // interrupt line to show beeing alive
    u8g2.setDrawColor(1);
    u8g2.drawLog(2,logbox_y+1,u8g2log);
    */
    //u8g2.sendBuffer();
    //LL_Log.printf("Touch %d %d %d %d\r\n", touchRead(14), touchRead(12), touchRead(15), touchRead(2));
    LL_Log.printf("Touch %d %d %d %d\r\n", touchRead(6), touchRead(7), touchRead(8), touchRead(2));
  }

  if(LL_Log.receiveLineAvailable()) {
    LL_Log.println(LL_Log.receiveLine);
    if(LL_Log.receiveLine[0]=='d') {

    }  
    if(LL_Log.receiveLine[0]=='w') {
      int addr, data;
      if(sscanf(&LL_Log.receiveLine[1],"%d %d",&addr, &data)>=2) {
        LL_Log.printf("Addr: %d, Data: %d\r\n", addr, data);
        face->Behavior.SetEmotion((eEmotions)addr /* moods[addr] */, (float)(data % 21) / 20.0 );
      }
    }
  }

  ArduinoOTA.handle();
  if(OTAinProgress == false) {
    face->Update();
  } 
}