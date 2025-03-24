/**
   The MIT License (MIT)
*/

#include "hackffm_badge_lib.h"

#include <MyCredsHackffm.h>      // Define WIFI_SSID and WIFI_PASSWORD here
#include <elapsedMillis.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>

#include "LL_Lib.h"
#include "helper.h" 


const char* hostname = "esp32-oled";
 
void setup() {
  HackFFMBadge.begin();

  LL_Log.begin(115200, 2222); // Serial baud, TCP Port

  Badge.drawBMP("/badge.bmp");
  delay(6000);

  //connectWifi(WIFI_SSID, WIFI_PASSWORD, hostname); 
  //while(1) delay(1);

  // Assign a weight to each emotion
  Badge.face().Behavior.GoToEmotion(eEmotions::Normal);
  Badge.face().Behavior.SetEmotion(eEmotions::Normal, 1.0);
  Badge.face().Behavior.SetEmotion(eEmotions::Happy, 0.0);
  Badge.face().Behavior.SetEmotion(eEmotions::Glee, 0.0);
  // Automatically switch between behaviours (selecting new behaviour randomly based on the weight assigned to each emotion)
  Badge.face().RandomBehavior = false;

  // Automatically blink
  Badge.face().RandomBlink = true;
  // Set blink rate
  Badge.face().Blink.Timer.SetIntervalMillis(4000);

  // Automatically choose a new random direction to look
  Badge.face().RandomLook = true;

}


void loop() {
  static elapsedMillis pressedTime = 0;
  static bool lastButState = true;
  static elapsedMillis blinkTimer = 0;

  Badge.update();

  Badge.drawFace();

  /*
    // Paint Face like TV raster lines
    u8g2.clearBuffer();
    Badge.face().UpdateBuffer();
    u8g2.setDrawColor(0);
    for(int y = 0; y < 64; y+=2) {
      u8g2.drawHLine(0,y,128);
    }
    u8g2.setDrawColor(1);
    u8g2.sendBuffer();
  */

  LL_Log.update(); 

  
  if(blinkTimer > 100) {
    static int emo = 0;
    blinkTimer = 0;

    if(lastButState != digitalRead(0)) {
      lastButState = digitalRead(0);
      if(lastButState == 0) {
        pressedTime = 0;
      } else {
        if(pressedTime > 1000) {
          LL_Log.println("Long Press");
          delay(1000);
          HackFFMBadge.powerOff();
        } else {
          LL_Log.println("Short Press");
          emo++;
          if(emo>=EMOTIONS_COUNT) emo = 0;
          Badge.face().Behavior.GoToEmotion((eEmotions)emo);
        }
      }
    }

    if(HackFFMBadge.isNewTouchDataAvailable()) {
      float x = (float)HackFFMBadge.touch[0].getTouchValue() / 100.0 - (float)HackFFMBadge.touch[1].getTouchValue() / 100.0;
      float y = (float)HackFFMBadge.touch[2].getTouchValue() / 100.0 - (float)HackFFMBadge.touch[3].getTouchValue() / 100.0;
   //   LL_Log.printf("Touch %d %d %d %d %f:%f\r\n", HackFFMBadge.touch[0].getLastTouchValue(), 
   //     HackFFMBadge.touch[1].getLastTouchValue(), HackFFMBadge.touch[2].getLastTouchValue(), 
   //     HackFFMBadge.touch[3].getLastTouchValue(),x,y);
      Badge.face().Look.LookAt(x, y); 
    }


    static uint8_t j = 0;
    j++;
    for (int i = 0; i < 2; i++) {
    //  HackFFMBadge.setAntennaLED( HackFFMBadge.freenoveAntennaLED->Wheel((i * 256 / 2 + j) & 255)); // crash...
    }
    Badge.setAntennaLED(j%32, j%16, j%20);
  }


  if(LL_Log.receiveLineAvailable()) {
    LL_Log.println(LL_Log.receiveLine);
    if(LL_Log.receiveLine[0]=='d') {
      int data;
      if(sscanf(&LL_Log.receiveLine[1],"%d",&data)>=1) {
        LL_Log.printf("Brigthness Data: %d\r\n",  data);
        u8g2.setContrast(data);
      }
    }  
    if(LL_Log.receiveLine[0]=='w') {
      int addr, data;
      if(sscanf(&LL_Log.receiveLine[1],"%d %d",&addr, &data)>=2) {
        LL_Log.printf("Addr: %d, Data: %d\r\n", addr, data);
        Badge.face().Behavior.SetEmotion((eEmotions)addr /* moods[addr] */, (float)(data % 21) / 20.0 );
      }
    }
  }

  ArduinoOTA.handle();
  if(OTAinProgress == false) {
  //  face->Update();
  } 

}