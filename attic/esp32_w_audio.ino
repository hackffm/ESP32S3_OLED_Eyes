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

//#define WAV_AUDIO_ACTIVE 1

#ifdef WAV_AUDIO_ACTIVE
  #include "AudioTools.h"
  //#include "knghtsng.h"
  //#include "alice.h"
  //#include "walle1.h"
  #include "happy.h"


  //Data Flow: MemoryStream -> EncodedAudioStream  -> PWMAudioOutput
  //Use 8000 for alice_wav and 11025 for knghtsng_wav
  AudioInfo info(11025, 1, 16);

  MemoryStream wav((const uint8_t*)happytree, wav_len);
  //MemoryStream wav(knghtsng_wav, knghtsng_wav_len);
  //MemoryStream wav(alice_wav, alice_wav_len);
  PWMAudioOutput pwm;          // PWM output 
  //EncodedAudioStream out(&pwm, new WAVDecoder()); // Decoder stream
  StreamCopy copier(pwm, wav, 60000);    // copy in to out

  Task task("mp3-copy", 10000, 1, 0);
#endif

const char* hostname = "esp32-oled";
 
void setup() {
  HackFFMBadge.begin();

  LL_Log.begin(115200, 2222); // Serial baud, TCP Port

  //connectWifi(WIFI_SSID, WIFI_PASSWORD, hostname); 
  //while(1) delay(1);

  // Assign a weight to each emotion
  Badge.face().Behavior.SetEmotion(eEmotions::Normal, 0.5);
  Badge.face().Behavior.SetEmotion(eEmotions::Happy, 0.3);
  Badge.face().Behavior.SetEmotion(eEmotions::Glee, 0.2);
  // Automatically switch between behaviours (selecting new behaviour randomly based on the weight assigned to each emotion)
  Badge.face().RandomBehavior = true;

  // Automatically blink
  Badge.face().RandomBlink = true;
  // Set blink rate
  Badge.face().Blink.Timer.SetIntervalMillis(4000);

  // Automatically choose a new random direction to look
  Badge.face().RandomLook = true;

 
  #ifdef WAV_AUDIO_ACTIVE
  // AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
    Badge.setPowerAudio(true);
    wav.begin();
    // out.begin();

    auto config = pwm.defaultConfig();
    config.copyFrom(info);
    config.start_pin = HackFFMBadge.pinAudio;
    pwm.begin(config);

    // task.begin([](){copier.copy();});
  #endif

}


void loop() {
  static elapsedMillis pressedTime = 0;
  static bool lastButState = true;
  static elapsedMillis blinkTimer = 0;

  Badge.update();

  u8g2.clearBuffer();
  Badge.face().UpdateBuffer();
  u8g2.sendBuffer();


  LL_Log.update(); 

  
  if(blinkTimer > 100) {
    static int emo = 0;
    blinkTimer = 0;

    #if 0
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
          face->Behavior.GoToEmotion((eEmotions)emo);
        }
      }
    }
    if(digitalRead(0)==0) {
      LL_Log.println("Push");
      emo++;
      if(emo>=EMOTIONS_COUNT) emo = 0;
      face->Behavior.GoToEmotion((eEmotions)emo);
    }
    //int logbox_y = u8g2.getDisplayHeight() - (U8LOG_HEIGHT * U8LOG_FONT_Y) - 3;
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

    #endif
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
  

  #ifdef WAV_AUDIO_ACTIVE
    if (wav) {
      copier.copy();
    } else if(0) {
      // after we are done we just print some info form the wav file
      auto info = pwm.audioInfo();
    //  LOGI("The audio rate from the wav file is %d", info.sample_rate);
    //  LOGI("The channels from the wav file is %d", info.channels);

      // restart from the beginning
      Serial.println("Restarting...");
      //delay(3000);
      face->Wait(3000);
      //out.begin();   // indicate that we process the WAV header
      wav.begin();       // reset actual position to 0
      pwm.begin();       // reset counters 
    }
  #endif
}