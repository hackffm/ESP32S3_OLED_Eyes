/**
   The MIT License (MIT) (but poisoned by GPL in the Face Library)
   Hackerspace-FFM e.V. Badge V1
   2025-04-20 Lutz Lisseck
*/

// Wifi credentials are in a MyCreds.h file that must reside in /<HOME>/.platformio/lib/MyCreds/MyCreds.h
// see attic/MyCreds.h for an example
#if defined __has_include
#  if __has_include (<MyCreds.h>)
#    include <MyCredsHackffm.h>  // Define WIFI_SSID and WIFI_PASSWORD here - see file in Attic for example
#  endif
#endif

#include <hackffm_badge_lib.h>
#include "menu.h"

#include "freertos-all.h"

#include "esp_system.h"
#include "esp_pm.h"

const int EYESTYLES_MAX = 2; // 0 = normal, 1 = also normal, 2 = raster lines
int currentEmotion = -1; // -1 = random behaviour, 0..EMOTIONS_COUNT = specific emotion
int currentEyeStyle = -1; // -2 = raster but changes, -1 = normal but changes, 0 = normal, 1 = also normal, 2 = raster lines
int durationShowEyes = 30000; // 30 seconds
int durationShowName = 5000;
int durationShowLogo = 2000;
int currentShow = 0; // 0 = eyes, 1 = name, 2 = logo
int currentShowDuration = 0;
elapsedMillis durationShowTimer = 0;
bool debugTouch = false;
uint32_t idleTime = 0;



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

//  if(Badge.filesystem.exists("/startsnd.txt")) Badge.playStartSound();

  Badge.drawBMP("/badge.bmp");
  elapsedMillis logoTimer = 0;
  while(logoTimer < 4000) {
    delay(10);
    if((HackFFMBadge.but0PressedSince() > 1000) /*|| (is_host_usb_device_connected() == true) */ ) {
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

  // setup2();
  Badge.playMP3("/boot.mp3");

  // Cast screen to this channel if file exists and contains the channel number
  Badge.txDisplaydata(Badge.readFile("/scrncast.chn").toInt());
}




// Shows eyes, logo, name and cycles through those
void doShow() {
  static int nameFade = 0;
  // Cycle when currentShowDuration is over
  if(durationShowTimer > currentShowDuration) {
    durationShowTimer = 0;
    currentShow++;
    if(currentShow >= 3) currentShow = 0;
    // set new currentShowDuration
    switch(currentShow) {
      case 0: 
        currentShowDuration = durationShowEyes; 
        if(currentEyeStyle < 0) {
          currentEyeStyle--;
          if(currentEyeStyle < -(EYESTYLES_MAX)) currentEyeStyle = -1; // reset to normal
        }
        break;
      case 1: currentShowDuration = durationShowName; nameFade = 128; break;
      case 2: currentShowDuration = durationShowLogo; break;
    }
  }

  // Renders actual show
  if(currentShowDuration > 0) {
    switch(currentShow) {
      case 0: // eyes, modify style if necessary
        Badge.face().UpdateBuffer(); // ~450us
        if(abs(currentEyeStyle) == 2) {
          // raster lines
          u8g2.setDrawColor(0);
          for(int y = 0; y < 64; y+=2) {
            u8g2.drawHLine(0,y,128);
          }
          u8g2.setDrawColor(1);
        }
        break;
      case 1: // Show user name, if not starting with $ then auto-size font
        u8g2.setDrawColor(1);
        if(Badge.userName[0] == '$') {
          String showuser = "$c$3$+$+$+$+";
          showuser.concat(Badge.userName);
          //showuser.replace(" ", "$n");
          Badge.drawString(showuser.c_str(),0,0,2,true);
        } else { 
          Badge.drawCenteredUTF8Text(Badge.getCleanName(Badge.userName).c_str(),true);
        }
        // Add fading effect
        if(nameFade > 0) {
          u8g2.setDrawColor(0);
          for(int y = 0; y < 64; y+=4) {
            u8g2.drawHLine(0,y,nameFade);
            u8g2.drawHLine(0,y+1,nameFade);
            u8g2.drawHLine(128-nameFade,y+2,nameFade);
            u8g2.drawHLine(128-nameFade,y+3,nameFade);
          }
          u8g2.setDrawColor(1);
          nameFade-=3;
        }
        break;
      case 2: // logo
        Badge.drawBMP("/badge.bmp", true);
        break;        
    }
  }

}


void loop() {

  Badge.update();  // <50us

  // Clear display buffer, paint first layer but dont send it to display
  u8g2.clearBuffer(); // <10us
  
  doShow(); // Paint eyes, name or logo
  
  LL_Log.update(); 

  doMenu(); // Overpaint if Menu is active

  float x = Badge.lastTouchX;
  float y = Badge.lastTouchY;
  x = constrain(x, -2.0, 2.0);
  y = constrain(y, -2.0, 2.0);
  Badge.face().Look.LookAt(-x, -y); 
  
  if(debugTouch) doDebugTouch();

  // Process commands from the command line
  processCommands();
  
  // Write to display
  if(Badge.OledAddress) u8g2.sendBuffer();  

  // Optionally transfer display data to external display (big Hackerspace-FFM mannequin)
  if(Badge.txDisplayChannel > 0) Badge.txDisplaydata(Badge.txDisplayChannel);

  // Idle this way disturbs USB serial connection - reduced CPU frequency instead to 80 Mhz
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
    if(LL_Log.receiveLine[0]=='W') {
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

      durationShowTimer = 0;
      currentShowDuration = durationShowName;
      currentShow = 1; // show name
    } 
    if(LL_Log.receiveLine[0]=='P') {
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
        //LL_Log.printf("Idle time: %d\r\n",  data);
        //idleTime = data;
        esp_pm_config_t pm_config = {
          .max_freq_mhz = 240,
          .min_freq_mhz = 80,
          .light_sleep_enable = false
        };
        
        switch(data) {
          case 0: pm_config.max_freq_mhz = 240; break;
          case 1: pm_config.max_freq_mhz = 160; break;
          case 2: pm_config.max_freq_mhz = 80; break;
          case 3: pm_config.max_freq_mhz = 40; break;
          case 4: pm_config.max_freq_mhz = 20; break;
          case 5: pm_config.max_freq_mhz = 10; break;
        }
        ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
      } else {
        LL_Log.printf(" Loop() - Free Stack Space: %d\n", uxTaskGetStackHighWaterMark(NULL));
        LL_Log.printf(" CPU_Freq: %d MHz\r\n", ESP.getCpuFreqMHz());
      }
     // LL_Log.printf(" u8g2: %d \r\n", u8g2.getBufferSize());
     // Badge.txDisplaydata(13);
    }
    if(LL_Log.receiveLine[0]=='D') {
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

      Badge.drawCenteredUTF8Text(&LL_Log.receiveLine[1]);
      delay(10000);
    }
  }
}