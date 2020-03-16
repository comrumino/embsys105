/*
    mp3Util.c
    Some utility functions for controlling the MP3 decoder.

    Developed for University of Washington embedded systems programming certificate
    
    2016/2 Nick Strathy wrote/arranged it
*/

#include "bsp.h"
#include "print.h"
#include "util.h"
#include "SD.h"
#include <Adafruit_FT6206.h>
#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_ILI9341.h>

#define PlayPauseLineLength 40
#define PlayPauseIcon_X (ILI9341_TFTWIDTH / 2) - (PlayPauseLineLength / 2)
#define PlayPauseIcon_Y ILI9341_TFTHEIGHT - (PlayPauseLineLength * 2)
/*
void renderCircle(Adafruit_ILI9341 lcdCtrl) {
  int currentcolor = ILI9341_RED;

  lcdCtrl.setCursor(PlayPauseIcon_X, PlayPauseIcon_Y);
  lcdCtrl.fillTriangle(PlayPauseIcon_X, PlayPauseIcon_Y,
                         PlayPauseIcon_X, PlayPauseIcon_Y + PlayPauseLineLength,
                         PlayPauseIcon_X + PlayPauseLineLength, PlayPauseIcon_Y + (PlayPauseLineLength / 2), currentcolor);

        //lcdCtrl.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
    
}*/

void renderRectangle(Adafruit_ILI9341 &lcdCtrl) {
      int currentcolor = ILI9341_RED;
  lcdCtrl.setCursor(PlayPauseIcon_X, PlayPauseIcon_Y);
  lcdCtrl.fillTriangle(PlayPauseIcon_X, PlayPauseIcon_Y,
                         PlayPauseIcon_X, PlayPauseIcon_Y + PlayPauseLineLength,
                         PlayPauseIcon_X + PlayPauseLineLength, PlayPauseIcon_Y + (PlayPauseLineLength / 2), currentcolor);

        //lcdCtrl.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
}
void renderTriangle(Adafruit_ILI9341 &lcdCtrl) {
    int currentcolor = ILI9341_RED;
  lcdCtrl.setCursor(PlayPauseIcon_X, PlayPauseIcon_Y);
  lcdCtrl.fillTriangle(PlayPauseIcon_X, PlayPauseIcon_Y,
                         PlayPauseIcon_X, PlayPauseIcon_Y + PlayPauseLineLength,
                         PlayPauseIcon_X + PlayPauseLineLength, PlayPauseIcon_Y + (PlayPauseLineLength / 2), currentcolor);

        //lcdCtrl.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
}


