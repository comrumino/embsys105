/*
    mp3/render.h
    Provides icon a consistent interface by isolating hardware dependent libraries such as adafruit-gfx
*/
#include <Adafruit_FT6206.h>
#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_ILI9341.h>
#ifndef __MP3RENDER_H
#define __MP3RENDER_H

//void renderCircle(Adafruit_ILI9341 lcdCtrl);
void renderRectangle(Adafruit_ILI9341 lcdCtrl);
void renderTriangle(Adafruit_ILI9341 lcdCtrl);

#endif