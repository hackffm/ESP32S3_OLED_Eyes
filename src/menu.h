#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <elapsedMillis.h>

// Gain access from main
extern const int EYESTYLES_MAX; // 0 = normal, 1 = also normal, 2 = raster lines
extern int currentEmotion; // -1 = random behaviour, 0..EMOTIONS_COUNT = specific emotion
extern int currentEyeStyle; // -2 = raster but changes, -1 = normal but changes, 0 = normal, 1 = also normal, 2 = raster lines
extern int durationShowEyes; // 30 seconds
extern int durationShowName;
extern int durationShowLogo;
extern int currentShow; // 0 = eyes, 1 = name, 2 = logo
extern int currentShowDuration;
extern elapsedMillis durationShowTimer;
extern bool debugTouch;

extern int menu_main_idx;

/**
 * Draw a line with selected text.
 * In the param line the text elements are stored as UTF8 
 * seperated by \n. selectedItem is the index of the item that
 * is currently selected and is highligthed by a box.
 **/
void drawSelectionLine(int x, int y, int spacing, const char *line_in, int selectedItem);

/**
 * Draw a white box with black text, aligned centered.
 **/
void drawCenteredText(int y, const char *text);

void utf8ToLazyAscii(char *out, const char *in);

void lazyAsciiToUtf8(char *out, const char *in);

void doMenu();

// Shows debug outputs for touch on screen and in log
void doDebugTouch();

#endif