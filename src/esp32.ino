/**
   The MIT License (MIT)
*/

#include <MyCredsHackffm.h>      // Define WIFI_SSID and WIFI_PASSWORD here
#include <elapsedMillis.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>

#include "Freenove_WS2812_Lib_for_ESP32.h"

#include "LL_Lib.h"
#include "helper.h" 

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
LL_Led LedBlue(38, true);

touchProcessor touch[4] = {6, 7, 8, 2};

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(2, 48, 0, TYPE_GRB);

void setup() {
  LL_Log.begin(115200, 2222); // Serial baud, TCP Port

  Wire.begin(5,4);
  Wire.setClock(400000);
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

  //connectWifi(WIFI_SSID, WIFI_PASSWORD, hostname); 
  WiFi.mode(WIFI_OFF);
  delay(1);

  // Create a new face
  face = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  // Assign the current expression
  face->Expression.GoTo_Normal();

  // Assign a weight to each emotion
  face->Behavior.SetEmotion(eEmotions::Normal, 1.0);
  //face->Behavior.SetEmotion(eEmotions::Happy, 0.3);
  //face->Behavior.SetEmotion(eEmotions::Glee, 0.2);
  // Automatically switch between behaviours (selecting new behaviour randomly based on the weight assigned to each emotion)
  face->RandomBehavior = false;

  // Automatically blink
  face->RandomBlink = true;
  // Set blink rate
  face->Blink.Timer.SetIntervalMillis(4000);

  // Automatically choose a new random direction to look
  face->RandomLook = false;

  // AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  wav.begin();
  // out.begin();

  auto config = pwm.defaultConfig();
  config.copyFrom(info);
  config.start_pin = 15;
  pwm.begin(config);

  // task.begin([](){copier.copy();});

  strip.begin();
  strip.setBrightness(255); 

}


elapsedMillis blinkTimer = 0;
void loop() {
  LL_Log.update(); LedBlue.update();

  if(blinkTimer > 100) {
    static int emo = 0;
    blinkTimer = 0;
    LedBlue.set(1, 1);
    if(digitalRead(0)==0) {
      LL_Log.println("Push");
      emo++;
      if(emo>=EMOTIONS_COUNT) emo = 0;
      face->Behavior.GoToEmotion((eEmotions)emo);
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
    for(int i=0; i<4; i++) {
      touch[i].update();
      if(touch[i].isTouched()) {
      }
    }
    float x = (float)touch[0].getTouchValue() / 100.0 - (float)touch[1].getTouchValue() / 100.0;
    float y = (float)touch[2].getTouchValue() / 100.0 - (float)touch[3].getTouchValue() / 100.0;
    LL_Log.printf("Touch %d %d %d %d %f:%f\r\n", touch[0].getTouchValue(), touch[1].getTouchValue(), touch[2].getTouchValue(), touch[3].getTouchValue(),x,y);
    face->Look.LookAt(x, y); 
    static uint8_t j = 0;
    j++;
    for (int i = 0; i < 2; i++) {
      strip.setLedColorData(i, strip.Wheel((i * 256 / 2 + j) & 255));
    }
    strip.show();
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
}