
#include <hackffm_badge_lib.h>
#include "menu.h"

/**
 * Draw a line with selected text.
 * In the param line the text elements are stored as UTF8 
 * seperated by \n. selectedItem is the index of the item that
 * is currently selected and is highligthed by a box.
 **/
void drawSelectionLine(int x, int y, int spacing, const char *line_in, int selectedItem) {
  int item = 0;
  int len = strlen(line_in);

  static int start_xpos = 0;
  int xpos = start_xpos; // = x;
  int ypos = y + u8g2.getFontAscent() - u8g2.getFontDescent();

  int linepos = 0;
  int linebegin = 0;
  int charpos = 0;

  int itemHeight = u8g2.getFontAscent() - u8g2.getFontDescent();
  int selectedItemWidth = 0;

  const int maxItems = 32;
  char *itemsStrings[maxItems];
  int itemsWidths[maxItems];
  int itemsWidthsSum = 0;

  if(len > 1020) len = 1020; // limit length to avoid overflow
  static char line[1024];
  memcpy(line, line_in, len + 1); // copy line_in to line, including \0
  line[len] = 0; // ensure null termination
  
  // separate items by \n, calculate widths
  while(linepos < len) {
    while((linepos < len) && (line[linepos] != '\n')) {
      linepos++; charpos++;
    }
    if(linepos < len) line[linepos++] = 0; // replace \n with 0
    if(charpos > 0) {
      itemsStrings[item] = &line[linebegin];
      itemsWidths[item] = u8g2.getUTF8Width(itemsStrings[item]);
      itemsWidthsSum += itemsWidths[item];
      if(item == selectedItem) {
        selectedItemWidth = itemsWidths[item];
      }  
      if(x == -1) {
        LL_Log.printf("Item %d: '%s' (%d)\n", item, itemsStrings[item], itemsWidths[item]);
      }
      if(item < maxItems) item++;
    }  
    charpos = 0;
    linebegin = linepos; 
  }

  // Draw items
  int idealX  = (u8g2.getDisplayWidth() - selectedItemWidth) / 2;

  for(int i = 0; i<item; i++) {
    // Draw black background for all items first
    u8g2.setDrawColor(0);
    u8g2.drawBox(xpos, y-1, itemsWidths[i]+2, itemHeight + 4);
    u8g2.setDrawColor(1); // set draw color to white for text
    
    u8g2.drawUTF8(xpos + 1, ypos, itemsStrings[i]);

    if(i == selectedItem) {
      u8g2.drawFrame(xpos, y-1, itemsWidths[i]+2, itemHeight + 4); // draw frame around selected item
      
      // Regulate xpos to fit into display width
      int idealDiff = xpos - idealX;
      if(idealDiff > 1) start_xpos -= min(idealDiff/2,8); // if selected item is too far right, move it left
      else if(idealDiff < -1) start_xpos += min(abs(idealDiff)/2,8); // if selected item is too far left, move it right 
    }

    xpos += itemsWidths[i] + spacing + 2;
  }



}

/**
 * Draw a white box with black text, aligned centered.
 **/
void drawCenteredText(int y, const char *text) {

  int ypos = y + u8g2.getFontAscent() - u8g2.getFontDescent();
  int itemWidth = u8g2.getUTF8Width(text);
  int itemHeight = u8g2.getFontAscent() - u8g2.getFontDescent();
  int xpos = (u8g2.getDisplayWidth() - itemWidth) / 2;
  
  u8g2.setDrawColor(1);
  u8g2.drawBox(xpos - 2, y - 1, itemWidth + 4, itemHeight + 4);
  u8g2.setDrawColor(0); 
  u8g2.drawUTF8(xpos, ypos, text);

}

void utf8ToLazyAscii(char *out, const char *in) {
  while (*in) {
    uint8_t c = (uint8_t)*in;

    if (c < 128) {
      // ASCII-Zeichen direkt kopieren
      *out++ = *in++;
    } else {
      // Versuch, bekannte UTF8-Sonderzeichen zu dekodieren
      if ((uint8_t)in[0] == 0xC3) {
        switch ((uint8_t)in[1]) {
          case 0xA4: *out++ = 25; in += 2; continue; // ä
          case 0xB6: *out++ = 26; in += 2; continue; // ö
          case 0xBC: *out++ = 27; in += 2; continue; // ü
          case 0x84: *out++ = 28; in += 2; continue; // Ä
          case 0x96: *out++ = 29; in += 2; continue; // Ö
          case 0x9C: *out++ = 30; in += 2; continue; // Ü
          case 0x9F: *out++ = 31; in += 2; continue; // ß
        }
      }
      if( ((uint8_t)in[0] == 0xE2) && ((uint8_t)in[1] == 0x97) 
        && ((uint8_t)in[2] == 0x80) ) {
          *out++ = 24; in += 3; continue;
      }

      // Unbekanntes UTF-8 Zeichen → ersetzen durch Space
      // Skip vollständiges UTF-8-Zeichen
      if ((c & 0xE0) == 0xC0) in += 2;           // 2 Byte
      else if ((c & 0xF0) == 0xE0) in += 3;      // 3 Byte
      else if ((c & 0xF8) == 0xF0) in += 4;      // 4 Byte
      else in++; // Fehlerfall, 1 Byte

      *out++ = 32; // Ersetze durch Space
    }
  }
  *out = '\0';
}

void lazyAsciiToUtf8(char *out, const char *in) {
  while (*in) {
    uint8_t c = (uint8_t)*in++;

    if (c < 24 || c > 31) {
      // Normales ASCII (0–24 oder 32–127)
      *out++ = c;
    } else {
      // Mapping zurück zu UTF-8-Zeichen
      switch (c) {
        case 24: *out++ = 0xE2; *out++ = 0x97; *out++ = 0x80; break;
        case 25: *out++ = 0xC3; *out++ = 0xA4; break; // ä
        case 26: *out++ = 0xC3; *out++ = 0xB6; break; // ö
        case 27: *out++ = 0xC3; *out++ = 0xBC; break; // ü
        case 28: *out++ = 0xC3; *out++ = 0x84; break; // Ä
        case 29: *out++ = 0xC3; *out++ = 0x96; break; // Ö
        case 30: *out++ = 0xC3; *out++ = 0x9C; break; // Ü
        case 31: *out++ = 0xC3; *out++ = 0x9F; break; // ß
      }
    }
  }
  *out = '\0';
}

int menu_main_idx = 0;
int menu_main_idx_next = 0; 
char menu_title[64];
elapsedMillis menu_title_timer = 0; // timer for title display, reset on change
char menu_selection_line[256];
char menu_selection_line_alt[256];
int  menu_selection_line_index = 0;
int  menu_selection_line_index_max = 0;



void doMenu() {

  // Get all touch and other inputs
  static bool prevLU = false;
  static bool prevLD = false;
  static bool prevRU = false;
  static bool prevRD = false;
  static bool prevLUS = false;
  static bool prevRUS = false;
  static elapsedMillis duraLD = 0; // increases while touch is pressed
  static elapsedMillis duraRD = 0;
  bool curLU = Badge.isTouchLU();
  bool curLD = Badge.isTouchLD();
  bool curRU = Badge.isTouchRU();
  bool curRD = Badge.isTouchRD();
  bool curLUS = Badge.isTouchLUStrong();
  bool curRUS = Badge.isTouchRUStrong();
  bool trigLU = (curLU && !prevLU); // trigger once on touch down
  bool trigLD = (curLD && !prevLD);
  bool trigRU = (curRU && !prevRU);
  bool trigRD = (curRD && !prevRD);
  bool trigLUS = (curLUS && !prevLUS);
  bool trigRUS = (curRUS && !prevRUS);
  uint32_t forBut0 = HackFFMBadge.but0PressedFor();  // 0 = no event or still pressed, otherwise time in ms of last press, cleared after read
  uint32_t duraBut0 = HackFFMBadge.but0PressedSince();
  if(!curLD) duraLD = 0;
  if(!curRD) duraRD = 0;
  prevLU = curLU; prevLD = curLD; prevRU = curRU; prevRD = curRD;
  prevLUS = curLUS; prevRUS = curRUS;

  if(menu_main_idx > 0) u8g2.clearBuffer(); // clear buffer if menu is active

  switch(menu_main_idx) {
    case 0: // *** Main state - usually no Menu is shown, but messages might be shown
      if(menu_title_timer > 1000) {
        menu_title[0] = 0; // no title
      }

      // But0: short press: Main Menu, long press (3s or more): Settings
      if((forBut0 == 0) && (duraBut0 > 8000)) {
        // very long press, exit to normal
        menu_main_idx_next = 0;
        menu_title[0] = 0;
        menu_title_timer = 0;
      } else if((forBut0 == 0) && (duraBut0 > 3000)) {
        // show "Settings", wait for release then enter settings menu
        menu_main_idx_next = 4;
        strcpy(menu_title, "Settings");
        menu_title_timer = 0;
      } else if((forBut0 == 0) && (duraBut0 > 0)) {
        // show "Main Menu", wait for release then enter main menu
        menu_main_idx_next = 1;
        strcpy(menu_title, "Main menu");
        menu_title_timer = 0;
      } else if((forBut0 > 0) && (duraBut0 < 40) && (menu_main_idx_next > 0)) {
        menu_main_idx = menu_main_idx_next; // change to next menu index
        menu_main_idx_next = 0; // reset next index
        menu_title_timer = 0;
      } else if(trigLUS) {
        // Cycle throug faces
        currentEmotion++;
        if(currentEmotion >= EMOTIONS_COUNT) currentEmotion = -1; // reset to random behaviour
        if(currentEmotion < 0) {
          Badge.face().RandomBehavior = true;
          strcpy(menu_title, "Random Emo");
        } else {
          Badge.face().RandomBehavior = false;
          Badge.face().Behavior.GoToEmotion((eEmotions)currentEmotion);
          strcpy(menu_title, Badge.face().Behavior.GetEmotionName((eEmotions)currentEmotion));
        }
        menu_title_timer = 0; // show text a while
        currentShow = 0; // jump to eyes
        durationShowTimer = 0; // reset timer for showing eyes
      } else if(trigRUS) {
        // Cycle through styles, skip style 0
        if(currentEyeStyle < 0) currentEyeStyle = 0;
        currentEyeStyle++;
        if(currentEyeStyle > EYESTYLES_MAX) currentEyeStyle = -1; // reset to normal
        switch(currentEyeStyle) {
          case -1:
            strcpy(menu_title, "Auto-change");
            break;
          case 1:
            strcpy(menu_title, "Normal");
            break;
          case 2:
            strcpy(menu_title, "Raster Lines");
            break;
        }
        menu_title_timer = 0; // show text a while
        currentShow = 0; // jump to eyes
        durationShowTimer = 0; // reset timer for showing eyes        
      } else if(trigLD && trigRD) {
        // Show name
        currentShow = 1;
        currentShowDuration = durationShowName;
        durationShowTimer = 0;
      }

      if(strlen(menu_title) > 0) {
        // Draw title
        u8g2.setFont(u8g2_font_DigitalDisco_tf);
        drawCenteredText(26, menu_title);
      } 
      break;

    case 1: // *** Main Menu: Door control, Touch debug
      menu_main_idx++;
      strcpy(menu_title, "Main menu");
      strcpy(menu_selection_line, "Back\nOpen Door\nClose Door\nTouch Debug\nPower off");
      menu_selection_line_index = 0;
      menu_selection_line_index_max = 4; 

    case 2:
      if(trigLD || trigRD) { // move cursor left or right
        if(trigLD && (menu_selection_line_index > 0)) menu_selection_line_index--;
        if(trigRD && (menu_selection_line_index < menu_selection_line_index_max)) menu_selection_line_index++;
      }
      if(trigRUS) {
        menu_main_idx = 0;
      }  
      if(trigLUS || (forBut0 > 10)) {
        // execute selected menu item
        switch(menu_selection_line_index) {
          case 0: // Back
            menu_main_idx = 0;
            break;
          case 1: // Try to open door;
            if(Badge.tryFindDoor() == true) {
              char buf[128]; sprintf(buf, "$!$c$8$+$+Door found %d:\n%s\nTry opening...", Badge.espNowRxRssi, Badge.door_name);
              LL_Log.println("Door found");
              Badge.drawString(buf);
              Badge.tryOpenDoor();
              delay(3000);
            } 
            //menu_main_idx = 0;
            break;
          case 2: // Try close door
            if(Badge.tryFindDoor() == true) {
              char buf[128]; sprintf(buf, "$!$c$8$+$+Door found %d:\n%s\nTry closing...", Badge.espNowRxRssi, Badge.door_name);
              LL_Log.println("Door found");
              Badge.drawString(buf);
              Badge.tryCloseDoor();
              delay(3000);
            } 
            //menu_main_idx = 0;
            break;
          case 3: // Toogle touch debug
            debugTouch = !debugTouch;
            if(debugTouch) {
              LL_Log.println("Debug Touch ON");
              Badge.drawString("$!$c$8$+$+$+$+$+$+$+Debug Touch ON");
            } else {
              LL_Log.println("Debug Touch OFF");
              Badge.drawString("$!$c$8$+$+$+$+$+$+$+Debug Touch OFF");
            }
            delay(1000);
            //menu_main_idx = 0;
            break;  
          case 4: // Power off
            Badge.drawString("$!$c$8$+$+$+$+$+$+$+Power off...");
            delay(1000);
            HackFFMBadge.powerOff();
            delay(1000);
            menu_main_idx = 0;
            break;            
          
        }
      }  

      u8g2.setFontMode(0);
      u8g2.setDrawColor(1);
      u8g2.setFont(u8g2_font_DigitalDisco_tf);
      u8g2.drawButtonUTF8(u8g2.getDisplayWidth()/2-1, 15, U8G2_BTN_HCENTER | 
        U8G2_BTN_SHADOW1 | U8G2_BTN_BW1 | U8G2_BTN_INV, 80,
        2, 2, menu_title);
      
      u8g2.setFont(u8g2_font_9x15_t_symbols);
      drawSelectionLine(0, 30, 0, menu_selection_line, menu_selection_line_index);


      u8g2.setDrawColor(1);
      u8g2.drawGlyph(2, 11, 0x2714);   u8g2.drawFrame(0, 0, 14, 14);    // ✔
      u8g2.drawGlyph(117, 10, 0x2715); u8g2.drawFrame(114, 0, 14, 14);  // ✕
     // u8g2.drawGlyph(2, 11+16, 0x25c2);   u8g2.drawFrame(0, 0+16, 14, 14);    // ◂
     // u8g2.drawGlyph(117, 11+16, 0x25b8); u8g2.drawFrame(114, 0+16, 14, 14);  // ▸
    //  u8g2.drawGlyph(21,  62, 0x25c5); // ◅
     // u8g2.drawGlyph(100, 62, 0x25bb); // ▻
      u8g2.drawGlyph(21,  62, 0x25c2); // ◂
      u8g2.drawGlyph(100, 62, 0x25b8); // ▸
      break;        

    case 4: // *** Settings Menu: Set name, FW Update
      menu_main_idx++;
      strcpy(menu_title, "Settings");
      strcpy(menu_selection_line, "Back\nSet name\nFW Update");
      menu_selection_line_index = 0;
      menu_selection_line_index_max = 2; 

    case 5:
      if(trigLD || trigRD) { // move cursor left or right
        if(trigLD && (menu_selection_line_index > 0)) menu_selection_line_index--;
        if(trigRD && (menu_selection_line_index < menu_selection_line_index_max)) menu_selection_line_index++;
      }
      if(trigRUS) {
        menu_main_idx = 0;
      }  
      if(trigLUS || (forBut0 > 10)) {
        // execute selected menu item
        switch(menu_selection_line_index) {
          case 0: // Back
            menu_main_idx = 0;
            break;
          case 1: // Set name
            menu_main_idx = 6;
            break;
          case 2: // FW Update
            LL_Log.println("Update firmware");
            Badge.drawString("$!$c$8$+$+$+$+$+$+$+Try update...");
            Badge.tryToUpdate();
            menu_main_idx = 0;
            break;
        }
      }  

      u8g2.setFontMode(0);
      u8g2.setDrawColor(1);
      u8g2.setFont(u8g2_font_DigitalDisco_tf);
      u8g2.drawButtonUTF8(u8g2.getDisplayWidth()/2-1, 15, U8G2_BTN_HCENTER | 
        U8G2_BTN_SHADOW1 | U8G2_BTN_BW1 | U8G2_BTN_INV, 80,
        2, 2, menu_title);
      
      u8g2.setFont(u8g2_font_9x15_t_symbols);
      drawSelectionLine(0, 30, 0, menu_selection_line, menu_selection_line_index);


      u8g2.setDrawColor(1);
      u8g2.drawGlyph(2, 11, 0x2714);   u8g2.drawFrame(0, 0, 14, 14);    // ✔
      u8g2.drawGlyph(117, 10, 0x2715); u8g2.drawFrame(114, 0, 14, 14);  // ✕
     // u8g2.drawGlyph(2, 11+16, 0x25c2);   u8g2.drawFrame(0, 0+16, 14, 14);    // ◂
     // u8g2.drawGlyph(117, 11+16, 0x25b8); u8g2.drawFrame(114, 0+16, 14, 14);  // ▸
    //  u8g2.drawGlyph(21,  62, 0x25c5); // ◅
     // u8g2.drawGlyph(100, 62, 0x25bb); // ▻
      u8g2.drawGlyph(21,  62, 0x25c2); // ◂
      u8g2.drawGlyph(100, 62, 0x25b8); // ▸
      break;  


    case 6: // Set name
      menu_main_idx++;
      strcpy(menu_title, "Set name");

      // get name and convert to lazy ASCII
      utf8ToLazyAscii(menu_selection_line_alt, Badge.userName);
      // prepare selection line with spaces
      {
        int i;
        int cpycnt = 24;
        for(i = 0; i < cpycnt; i++) {
          menu_selection_line[i*2] = ' ';
          menu_selection_line[i*2+1] = '\n';
        } 
        menu_selection_line[i*2] = 0; // null terminate
        menu_selection_line[i*2+1] = 0; // null terminate
        for(i = 0; i < cpycnt; i++) {
          char c = menu_selection_line_alt[i];
          if(c == 0) break; // end of string
          menu_selection_line[i*2] = c; // copy character
        }        
      }     

      //strcpy(menu_selection_line, "N\na\nm\ne\n \n \n \n \n \n \n \n ");

      menu_selection_line_index = 0;
      menu_selection_line_index_max = strlen(menu_selection_line)/2;
 
    case 7:  // Set name
      if(trigRD || (duraRD > 500) || trigLD || (duraLD > 500)) { // cycle through characters upwards
        if((menu_selection_line_index >= 0) && 
          (menu_selection_line_index <= menu_selection_line_index_max)) {
          uint8_t c = (uint8_t)menu_selection_line[menu_selection_line_index*2]; 
          if(curRD && (c < 127)) c++;
          if(curLD && (c > 24)) c--;
          menu_selection_line[menu_selection_line_index*2] = (char)c;
        }
      }
      if(trigLU || trigRU) { // move cursor left or right
        if((menu_selection_line_index >= 0) && 
          (menu_selection_line_index < menu_selection_line_index_max)) {
          uint8_t c = (uint8_t)menu_selection_line[menu_selection_line_index*2]; 
          if(c == 24) {
            // delete actual character by moving right part of string to left
            for(int i = menu_selection_line_index; i < menu_selection_line_index_max; i++) {
              menu_selection_line[i*2] = menu_selection_line[(i+1)*2];
              menu_selection_line[i*2+1] = menu_selection_line[(i+1)*2+1];
            }
            menu_selection_line[menu_selection_line_index_max*2] = ' ';
          }
        }
        if(trigLU && (menu_selection_line_index > 0)) menu_selection_line_index--;
        if(trigRU && (menu_selection_line_index < menu_selection_line_index_max)) menu_selection_line_index++;
      }

      if(trigRUS) { // Cancel button
        menu_main_idx = 0;
      }  
      if(trigLUS) { // OK button
        if((menu_selection_line_index >= 0) && 
          (menu_selection_line_index < menu_selection_line_index_max)) {
          uint8_t c = (uint8_t)menu_selection_line[menu_selection_line_index*2]; 
          if(c == 24) {
            // delete actual character by moving right part of string to left
            for(int i = menu_selection_line_index; i < menu_selection_line_index_max; i++) {
              menu_selection_line[i*2] = menu_selection_line[(i+1)*2];
              menu_selection_line[i*2+1] = menu_selection_line[(i+1)*2+1];
            }
            menu_selection_line[menu_selection_line_index_max*2] = ' ';
          }
        }
        int lastSpace = -1;
        for(int i = 0; i < 120; i++) {
          char c = (char)menu_selection_line[i*2];
          menu_selection_line[i] = c;
          if(c == 0) break; // end of string
          if(c == ' ') {
            if(lastSpace < 0) lastSpace = i; 
          } else lastSpace = -1;
        }
        if(lastSpace >= 0) {
          // trim away trailing spaces
          menu_selection_line[lastSpace] = 0; 
        }
        lazyAsciiToUtf8(menu_selection_line_alt, menu_selection_line);
        LL_Log.printf("Set name: '%s'\n", menu_selection_line_alt);
        strcpy(Badge.userName, menu_selection_line_alt);

        LL_Log.printf("Bade name: %s\r\n",  Badge.userName);
        Badge.writeFile("/name.txt", Badge.userName);
        Badge.genDoorName();
        Badge.saveKey();

        menu_main_idx = 0;
        durationShowTimer = 0;
        currentShow = 1; // show name
      }  

      u8g2.setFontMode(0);
      u8g2.setDrawColor(1);
      u8g2.setFont(u8g2_font_DigitalDisco_tf);
      u8g2.drawButtonUTF8(u8g2.getDisplayWidth()/2-1, 15, U8G2_BTN_HCENTER | 
        U8G2_BTN_SHADOW1 | U8G2_BTN_BW1 | U8G2_BTN_INV, 80,
        2, 2, "Set name");
      
      u8g2.setFont(u8g2_font_9x15_t_symbols);
      lazyAsciiToUtf8(menu_selection_line_alt, menu_selection_line);
      drawSelectionLine(0, 30, 0, menu_selection_line_alt, menu_selection_line_index);

      u8g2.setDrawColor(1);
      u8g2.drawGlyph(2, 11, 0x2714);   u8g2.drawFrame(0, 0, 14, 14);    // ✔
      u8g2.drawGlyph(117, 10, 0x2715); u8g2.drawFrame(114, 0, 14, 14);  // ✕
      
      u8g2.drawGlyph(2, 11+16, 0x25c2);   u8g2.drawFrame(0, 0+16, 14, 14);    // ◂
      u8g2.drawGlyph(117, 11+16, 0x25b8); u8g2.drawFrame(114, 0+16, 14, 14);  // ▸

      u8g2.drawGlyph(21,  62, 0x25c5); // ◅
      u8g2.drawGlyph(100, 62, 0x25bb); // ▻
      break; 
  }
}  


// Shows debug outputs for touch on screen and in log
void doDebugTouch() {
    float x = Badge.lastTouchX;
    float y = Badge.lastTouchY;
    LL_Log.printf("Touch: Last %d, Avg %d, Val %d , ",
      Badge.touch[0].getLastTouchValue(), Badge.touch[0].getTouchAvgValue(), Badge.touch[0].getTouchValue());
    LL_Log.printf("Touch LU:%.2f LD:%.2f RU:%.2f RD:%.2f %.2f:%.2f\r\n", HackFFMBadge.lastTouchLU, HackFFMBadge.lastTouchLD,
      HackFFMBadge.lastTouchRU, HackFFMBadge.lastTouchRD, x, y);
    x = constrain(x, -2.0, 2.0);
    y = constrain(y, -2.0, 2.0);
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