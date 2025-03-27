/**
   The MIT License (MIT) (but poisoned by GPL in the Face Library)
   Hackerspace-FFM e.V. Badge V1
   2025-03-25 Lutz Lisseck
*/

// Wifi credentials are in a MyCreds.h file that must reside in /<HOME>/.platformio/lib/MyCreds/MyCreds.h
// see attic/MyCredsHackffm.h for an example
#if defined __has_include
#  if __has_include (<MyCreds.h>)
#    include <MyCreds.h>  // Define WIFI_SSID and WIFI_PASSWORD here - see file in Attic for example
#  endif
#endif

#include <hackffm_badge_lib.h>

const char* hostname = "hackffm-badge";
char badgeUserName[128] = "$cHackFFM$nBadge";
 
void setup() {
  HackFFMBadge.begin();

  LL_Log.begin(115200, 2222); // Serial baud (not used), TCP Port

  Badge.drawBMP("/badge.bmp");
  elapsedMillis logoTimer = 0;
  while(logoTimer < 6000) {
    delay(10);
    if(HackFFMBadge.but0PressedFor() > 1000) {
      #ifdef WIFI_SSID
      LL_Log.println("Long Press");
      delay(10);
      HackFFMBadge.connectWifi(WIFI_SSID, WIFI_PASSWORD, hostname);
      char buf[128]; sprintf(buf, "$!$c$8$+$+$+IP: %s", WiFi.localIP().toString().c_str());
      Badge.drawString(buf);
      Badge.setBoardLED(2, 0, 8); // pink
      delay(8000);
      #endif
    }
  }
  

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

int CurrentAction = 0;
int CurrentActionDuration = 1000;
elapsedMillis CurrentActionTimer = 0;


void loop() {
  static elapsedMillis actionTimer = 0;

  Badge.update();  // <50us
  
  if(CurrentActionTimer > CurrentActionDuration) {
    CurrentActionTimer = 0;
    CurrentAction++;
    if(CurrentAction > 3) CurrentAction = 0;
  }
  switch(CurrentAction) {
    case 0: 
      CurrentActionDuration = 5000;
      Badge.drawFace();
      break;
    case 1: 
      CurrentActionDuration = 1000;
      Badge.drawBMP("/badge.bmp");
      break;
    case 2: 
      CurrentActionDuration = 5000;
      // Paint Face like TV raster lines
      u8g2.clearBuffer(); // <10us
      Badge.face().UpdateBuffer(); // ~450us
      u8g2.setDrawColor(0);
      for(int y = 0; y < 64; y+=2) {
        u8g2.drawHLine(0,y,128);
      }
      u8g2.setDrawColor(1);
      u8g2.sendBuffer(); // ~17.1ms
      break;
    case 3: 
      CurrentActionDuration = 5000;
      u8g2.clearBuffer(); 
      u8g2.setDrawColor(1);
      //Badge.face().UpdateBuffer();
      Badge.drawString(badgeUserName);
      
      u8g2.sendBuffer(); 

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

    if(pressedFor > 1000) {
      LL_Log.println("Long Press");
      delay(1000);
      HackFFMBadge.powerOff();
    } else if(pressedFor > 0) {
      LL_Log.println("Short Press");
      emo++;
      if(emo>=EMOTIONS_COUNT) emo = 0;
      Badge.face().Behavior.GoToEmotion((eEmotions)emo);
    }

    if(HackFFMBadge.isNewTouchDataAvailable()) {
      float x = (float)HackFFMBadge.touch[0].getTouchValue() / 100.0 - (float)HackFFMBadge.touch[1].getTouchValue() / 100.0;
      float y = (float)HackFFMBadge.touch[2].getTouchValue() / 100.0 - (float)HackFFMBadge.touch[3].getTouchValue() / 100.0;
      LL_Log.printf("Touch LU:%d LD:%d RU:%d RD:%d %f:%f\r\n", HackFFMBadge.touch[0].getTouchValue(), 
        HackFFMBadge.touch[1].getTouchValue(), HackFFMBadge.touch[2].getTouchValue(), 
        HackFFMBadge.touch[3].getTouchValue(),x,y);
      Badge.face().Look.LookAt(x, y); 
    }

    // Animate antenna LED
    static uint8_t j = 0;
    j++;
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
    if(LL_Log.receiveLine[0]=='u') {
      char data[128];
      if(sscanf(&LL_Log.receiveLine[1],"%127[^\n]",&data)>=1) {
        strcpy(badgeUserName, data);
        LL_Log.printf("Bade name: %s\r\n",  badgeUserName);

      }
    } 
  }


}