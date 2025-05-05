/**
   The MIT License (MIT) (but poisoned by GPL in the Face Library)
   Hackerspace-FFM e.V. Badge V1
   2025-04-20 Lutz Lisseck
*/

// Wifi credentials are in a MyCreds.h file that must reside in /<HOME>/.platformio/lib/MyCreds/MyCreds.h
// see attic/MyCreds.h for an example
#if defined __has_include
#  if __has_include (<MyCreds.h>)
#    include <MyCreds.h>  // Define WIFI_SSID and WIFI_PASSWORD here - see file in Attic for example
#  endif
#endif

#include <hackffm_badge_lib.h>

#include "freertos-all.h"

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceLittleFS.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"


I2SStream i2s;
PWMAudioOutput pwm; 
MP3DecoderHelix decoder;
const char *startFilePath="/";
const char* ext="mp3";
audio_tools::AudioSourceLittleFS source(startFilePath, ext);
AudioPlayer player(source, i2s /* pwm */, decoder);

SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
I2SStream out; 
StreamCopy copier(out, sound);                             // copies sound into i2s

void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}

void setup2() {
  AudioInfo info(44100, 2, 16);
  Badge.setPowerAudio(true);
  while(Serial.available()) Serial.read();

  LL_Log.println("Audio start");
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  cfg.signal_type = PDM; 
  cfg.pin_data = HackFFMBadge.pinAudio;
  cfg.pin_bck = 17; // need a clock pin for PDM even if not used
  i2s.begin(cfg);

  auto config = pwm.defaultConfig();
  config.copyFrom(info);
  config.start_pin = HackFFMBadge.pinAudio;
//  pwm.begin(config);
  

  // setup player
  //source.setFileFilter("*Bob Dylan*");
  player.setMetadataCallback(printMetaData);
  player.setVolume(1.0);
  player.begin();
  
  while(!Serial.available() && player.isActive()) {
    player.copy();
  }
  LL_Log.println("Audio done");
  Badge.setPowerAudio(false);

}

void setup2a(void) {  
  AudioInfo info(44100, 2, 16);
  // Open Serial 
  Badge.setPowerAudio(true);
  while(Serial.available()) Serial.read();
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.signal_type = PDM; 
  config.pin_data = 6; // Here comes the audio (but Left+Right!)
  config.pin_bck = 17; // need a clock pin for PDM even if not used
  config.copyFrom(info); 
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");

  while(!Serial.available()) copier.copy();
  LL_Log.println("Audio done");
  Badge.setPowerAudio(false);
}



// This sets Arduino Stack Size - comment this line to use default 8K stack size
SET_LOOP_TASK_STACK_SIZE(16*1024); // 16KB

cpp_freertos::Task task1("AntennaLED", 2000, 1, [](){
  // Animate antenna LED
  static uint8_t j = 0;
  static int once = 100;
  j++;
  Badge.setAntennaLED(j%32, j%16, j%20);
  delay(50);
  if(once) {
    once--;
    if(once == 0) {
      Serial.printf(" Task1() - Free Stack Space: %d\n", uxTaskGetStackHighWaterMark(NULL));
    }
  }
});

void setup() {
  HackFFMBadge.begin();

  if(Badge.filesystem.exists("/startsnd.txt")) Badge.playStartSound();

  Badge.drawBMP("/badge.bmp");
  elapsedMillis logoTimer = 0;
  while(logoTimer < 6000) {
    delay(10);
    if(HackFFMBadge.but0PressedSince() > 1000) {
      #ifdef WIFI_SSID
      LL_Log.println("Long Press");
      Badge.drawString("$!$c$8$+$+$+$+$+$+$+Connecting$nto WiFi..");
      delay(10);
      HackFFMBadge.connectWifi(WIFI_SSID, WIFI_PASSWORD);
      char buf[128]; sprintf(buf, "$!$c$6$+$+$+$+$+$+IP: %s", WiFi.localIP().toString().c_str());
      Badge.drawString(buf);
      Badge.setBoardLED(2, 0, 8); // pink
      delay(8000);
      Badge.but0PressedFor(); // clear button press
      break;
      #endif
    }
  }

  
  task1.Start(1);

  setup2();
}



int CurrentAction = 0;
int CurrentActionDuration = 1000;
elapsedMillis CurrentActionTimer = 0;
uint32_t idleTime = 0;
bool debugTouch = false;

void loop() {
  static elapsedMillis actionTimer = 0;

  Badge.update();  // <50us

  // Clear display buffer, paint first layer but dont send it to display
  u8g2.clearBuffer(); // <10us
  
  if(CurrentActionTimer > CurrentActionDuration) {
    CurrentActionTimer = 0;
    CurrentAction++;
    if(CurrentAction > 3) CurrentAction = 0;
  }
  switch(CurrentAction) {
    case 0: 
      CurrentActionDuration = 30000;
      Badge.face().UpdateBuffer(); // ~450us
      break;
    case 1: 
      CurrentActionDuration = 1000;
      Badge.drawBMP("/badge.bmp", true);
      break;
    case 2: 
      CurrentActionDuration = 30000;
      // Paint Face like TV raster lines
      
      Badge.face().UpdateBuffer(); // ~450us
      u8g2.setDrawColor(0);
      for(int y = 0; y < 64; y+=2) {
        u8g2.drawHLine(0,y,128);
      }
      u8g2.setDrawColor(1);

      break;
    case 3: 
      CurrentActionDuration = 3000;
       
      u8g2.setDrawColor(1);
      //Badge.face().UpdateBuffer();
      String showuser = "$c$3$+$+$+$+";
      showuser.concat(Badge.userName);
      showuser.replace(" ", "$n");
      Badge.drawString(showuser.c_str(),0,0,2,true);

      break;
  }
  
  LL_Log.update(); 

  
  if(actionTimer > 100) {
    static int emo = 0;
    actionTimer = 0;

    // check button on MCU module itself (beside RESET button)
    uint32_t pressedFor = HackFFMBadge.but0PressedFor();
    uint32_t pressedSince = HackFFMBadge.but0PressedSince();
    if((pressedFor > 0) || (pressedSince > 0)) {
      LL_Log.printf("PressedFor: %d, PressedSince: %d\r\n", pressedFor, pressedSince);
    }

    if(pressedFor > 10000) {
      // Try update firmware from web
      Badge.drawString("$!$c$8$+$+$+$+$+$+$+Try update...");
      Badge.tryToUpdate();
    } else if(pressedFor > 5000) {
      Badge.drawString("$!$c$8$+$+$+$+$+$+$+Power off...");
      delay(1000);
      HackFFMBadge.powerOff();
    } else if(pressedFor > 1000) {
      LL_Log.println("Long Press");
      debugTouch = !debugTouch;
      if(debugTouch) {
        LL_Log.println("Debug Touch ON");
        Badge.drawString("$!$c$8$+$+$+$+$+$+$+Debug Touch ON");
      } else {
        LL_Log.println("Debug Touch OFF");
        Badge.drawString("$!$c$8$+$+$+$+$+$+$+Debug Touch OFF");
      }
      delay(1000);
    } else if(pressedFor > 0) {
      LL_Log.println("Short Press");
      emo++;
      if(emo>=EMOTIONS_COUNT) emo = 0;
      Badge.face().RandomBehavior = false; // Stop random emotions
      Badge.face().Behavior.GoToEmotion((eEmotions)emo);
      LL_Log.printf("Set emotion: %s\r\n", Badge.face().Behavior.GetEmotionName((eEmotions)emo));
    }
  }

  /*  if(HackFFMBadge.isNewTouchDataAvailable()) */ 
  {
      float x = Badge.lastTouchX;
      float y = Badge.lastTouchY;
      if(y < -3.0) {
        CurrentActionTimer = 0;
        CurrentAction = 3; // show name
        if(Badge.tryFindDoor() == true) {
          LL_Log.println("Door found");
          Badge.drawString("$!$c$8$+$+$+$+$+$+$+Door found");
          Badge.tryCloseDoor();
          delay(1000);
        } 
      }
      if(y > 3.0) {
        CurrentActionTimer = 0;
        CurrentAction = 3; // show name
        if(Badge.tryFindDoor() == true) {
          LL_Log.println("Door found");
          Badge.drawString("$!$c$8$+$+$+$+$+$+$+Door found");
          Badge.tryOpenDoor();
          delay(1000);
        } 
      }
      x = constrain(x, -2.0, 2.0);
      y = constrain(y, -2.0, 2.0);
      if(debugTouch) {
        LL_Log.printf("Touch LU:%.2f LD:%.2f RU:%.2f RD:%.2f %.2f:%.2f\r\n", HackFFMBadge.lastTouchLU, HackFFMBadge.lastTouchLD,
          HackFFMBadge.lastTouchRU, HackFFMBadge.lastTouchRD, x, y);
        u8g2.setDrawColor(0);
        u8g2.drawCircle(int(x*128.0+64), int(y*64.0+32), 4);
        u8g2.setDrawColor(1);
        u8g2.drawCircle(int(x*128.0+64), int(y*64.0+32), 5);
        if(Badge.isTouchLU()) u8g2.drawBox(0, 0, 2, 32);
        if(Badge.isTouchLD()) u8g2.drawBox(0, 32, 2, 32);
        if(Badge.isTouchRU()) u8g2.drawBox(126, 0, 2, 32);
        if(Badge.isTouchRD()) u8g2.drawBox(126, 32, 2, 32);
        if(Badge.isTouchLUStrong()) u8g2.drawBox(2, 0, 8, 8);
        if(Badge.isTouchRUStrong()) u8g2.drawBox(118, 0, 8, 8);
           
      }
      Badge.face().Look.LookAt(-x, -y); 
    }


  

  // Process commands from the command line
  processCommands();
  
  // Write to display
  u8g2.sendBuffer();  
  
  if(idleTime > 0) {
    esp_sleep_enable_timer_wakeup(idleTime * 1000);
    esp_light_sleep_start();
  }
}


// Process commands from the command line
void processCommands() {
  if(LL_Log.receiveLineAvailable()) {
    LL_Log.println(LL_Log.receiveLine);
    if(LL_Log.receiveLine[0]=='B') {
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
    if(LL_Log.receiveLine[0]=='n') {
      char data[128];
      if(sscanf(&LL_Log.receiveLine[1],"%127[^\n]",&data)>=1) {
        strcpy(Badge.userName, data);
        LL_Log.printf("Bade name: %s\r\n",  Badge.userName);
        Badge.writeFile("/name.txt", Badge.userName);
        Badge.genDoorName();
        Badge.saveKey();
      } else {
        LL_Log.printf("Bade name: %s\r\n",  Badge.userName);
      }

      CurrentActionTimer = 0;
      CurrentAction = 3; // show name
    } 
    if(LL_Log.receiveLine[0]=='p') {
      char data[128];
      if(sscanf(&LL_Log.receiveLine[1],"%127[^\n]",&data)>=1) {
        bool gkstat = Badge.genKey(data);
        if(gkstat) {
          LL_Log.printf("Key set to: %s\r\n",  data);
          Badge.saveKey();
        } else {
          LL_Log.printf("GenKey failed: %s\r\n",  data);
        }
      } else {
        char buf[70];
        bytes2hex(Badge.door_pubkey, 32, buf);
        LL_Log.printf("Public key: %s\r\n",  buf);
      }

    } 
    if(LL_Log.receiveLine[0]=='i') {
      int data;
      if(sscanf(&LL_Log.receiveLine[1],"%d",&data)>=1) {
        LL_Log.printf("Idle time: %d\r\n",  data);
        idleTime = data;
      } else {
        LL_Log.printf(" Loop() - Free Stack Space: %d\n", uxTaskGetStackHighWaterMark(NULL));
      }
    }
    if(LL_Log.receiveLine[0]=='d') {
      char data[128];
      if(sscanf(&LL_Log.receiveLine[1],"%127[^\n]",&data)>=1) {
        String del_name = "/" + String(data);
        LL_Log.printf("Delete file: %s\r\n",  del_name.c_str());
        if(Badge.filesystem.exists(del_name.c_str())) {
          if(Badge.filesystem.remove(del_name.c_str())) {
            LL_Log.printf("File %s deleted\r\n", del_name.c_str());
          } else {
            LL_Log.printf("File %s delete failed\r\n", del_name.c_str());
          }
         } else {
          LL_Log.printf("File %s not found\r\n", del_name.c_str());
        }
      }
    }
    if(LL_Log.receiveLine[0]=='a') {
      setup2();
    }
  }
}