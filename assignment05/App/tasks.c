/************************************************************************************

Copyright (c) 2001-2016  University of Washington Extension.

Module Name:

    tasks.c

Module Description:

    The tasks that are executed by the test application.

2016/2 Nick Strathy adapted it for NUCLEO-F401RE

************************************************************************************/
#include <stdarg.h>

#include "bsp.h"
#include "mp3/util.h"
#include "print.h"

#include <Adafruit_FT6206.h>
#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_ILI9341.h>

Adafruit_ILI9341 lcdCtrl = Adafruit_ILI9341(); // The LCD controller

Adafruit_FT6206 touchCtrl = Adafruit_FT6206(); // The touch controller
const uint32_t DISPLAY_QUEUE_SIZE = 32;
const uint32_t TS_BUFFER_SIZE = DISPLAY_QUEUE_SIZE;
const TS_Point tsReleasePseudoPoint = TS_Point(0xffff, 0xffff, 0xffff);
//
const uint32_t appTouchReady = 0x1; // 0000 0001
const uint32_t appDisplayReady = 0x2; // 0000 0010
const uint32_t appMp3StreamReady = 0x4; // 0000 0100

extern const uint32_t mp3MessagePlay;//const uint32_t mp3MessagePlay = 0x1;
extern const uint32_t mp3MessagePause;// const uint32_t mp3MessagePause = 0x2;
extern const uint32_t mp3MessageSkip;// const uint32_t mp3MessagePause = 0x2;

const int activecolor = ILI9341_RED;
const int inactivecolor = ILI9341_BLACK;
uint32_t lastActivityTime = 0;
uint32_t maxActivityRateInTicks = 400; // doherty threshold
// touch -> mp3 -> Lcd
OS_EVENT *mp3StreamMbox;
OS_EVENT *activityQueue;
OS_EVENT *touchMbox; // external touch interrupt

static TS_Point tsPointBuffer[TS_BUFFER_SIZE];
static TS_Point *pTsPointBuffer = &tsPointBuffer[0];
static TS_Point *pStartTsPointBuffer = &tsPointBuffer[0];
static TS_Point *pEndTsPointBuffer = &tsPointBuffer[TS_BUFFER_SIZE - 1];

OS_FLAG_GRP *AppStatus;
// TODO: Consider using event flags w/ interrupt based touch
//      see https://doc.micrium.com/display/osiiidoc/Event+Flags "Using Event Flags"
// OS_FLAG_GRP *TouchScreenStatus;
// OS_FLAG_GRP *TouchScreenEvents;
//
#define TouchMarginOfSafety 5
#define PlayPauseLineLength 34
#define SkipLineLength 34
#define PlayPauseIcon_X (ILI9341_TFTWIDTH / 2)
#define PlayPauseIcon_Y ILI9341_TFTHEIGHT - (PlayPauseLineLength * 2)
#define SkipIcon_X (PlayPauseIcon_X + 72)
#define SkipIcon_Y PlayPauseIcon_Y


bool touchedNeighborhood(TS_Point p, int iconX, int iconY, int neighborhood) {
    bool touched = iconX - (neighborhood/2) - TouchMarginOfSafety <= p.x;
    touched &= p.x <= iconX + (neighborhood/2) + TouchMarginOfSafety;
    touched &= iconY - (neighborhood/2) -TouchMarginOfSafety <= p.y ;
    touched &= p.y <= iconY + (neighborhood/2) + TouchMarginOfSafety;
    touched &= p.y != 0 && p.x != 0;
    return touched;
}
bool touchedPausePlay(TS_Point p) {
    return touchedNeighborhood(p, PlayPauseIcon_X, PlayPauseIcon_Y, PlayPauseLineLength);
}
bool touchedSkip(TS_Point p) {
    return touchedNeighborhood(p, SkipIcon_X, SkipIcon_Y, SkipLineLength);
}


void renderPausePlayCircle(Adafruit_ILI9341 lcdCtr, int fillcolor) {
  lcdCtrl.drawCircle(PlayPauseIcon_X, PlayPauseIcon_Y, PlayPauseLineLength, fillcolor);
}

void renderPlay(Adafruit_ILI9341 lcdCtrl, int fillcolor) {
  lcdCtrl.fillTriangle(PlayPauseIcon_X - 7, PlayPauseIcon_Y - 12,
                       PlayPauseIcon_X - 7, PlayPauseIcon_Y + 12,
                       PlayPauseIcon_X + 14, PlayPauseIcon_Y, fillcolor);    
}

void renderPause(Adafruit_ILI9341 lcdCtrl, int fillcolor) {
  lcdCtrl.fillRect(PlayPauseIcon_X - 10 , PlayPauseIcon_Y - 12, 7, 24, fillcolor);    
  lcdCtrl.fillRect(PlayPauseIcon_X + 3, PlayPauseIcon_Y - 12, 7, 24, fillcolor);
}
void renderSkip(Adafruit_ILI9341 lcdCtrl, int fillcolor) {
  lcdCtrl.fillTriangle(PlayPauseIcon_X + 61, PlayPauseIcon_Y - 12,
                       PlayPauseIcon_X + 61, PlayPauseIcon_Y + 12,
                       PlayPauseIcon_X + 82, PlayPauseIcon_Y, fillcolor);
    lcdCtrl.fillRect(PlayPauseIcon_X + 78, PlayPauseIcon_Y - 12, 7, 24, fillcolor);

}

//
//
//
//
//
//
#define PENRADIUS 3
long MapTouchToScreen(long coord, long dimension) {
    return dimension - coord;
}

long MapTouchToScreen(long x, long in_min, long in_max, long out_min, long out_max) {
    // adjust coordinates to account for rotation
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#include "train_crossing.h"

#define BUFSIZE 256

/************************************************************************************

   Allocate the stacks for each task.
   The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h

************************************************************************************/

static OS_STK TouchEventTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK ActivityTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK Mp3StreamTaskStk[APP_CFG_TASK_START_STK_SIZE];


// Useful functions
void PrintToLcdWithBuf(char *buf, int size, char *format, ...);

// uCOS task functions
void Mp3StreamTask(void *pdata);
void ActivityTask(void *pdata);
void TouchEventTask(void *pdata);

// Globals
BOOLEAN nextSong = OS_FALSE;

/************************************************************************************

   This task is the initial task running, started by main(). It starts
   the system tick timer and creates all the other tasks. Then it deletes itself.

************************************************************************************/
void StartupTask(void *pdata) {
    char buf[BUFSIZE];

    // Start the system tick
    PrintWithBuf(buf, BUFSIZE, "StartupTask: Starting timer tick\n");
    OS_CPU_SysTickInit(OS_TICKS_PER_SEC);

    // Initialize messaging
    mp3StreamMbox = OSMboxCreate((void *)&mp3MessagePause);
    activityQueue = OSQCreate(NULL, DISPLAY_QUEUE_SIZE);
    touchMbox = OSMboxCreate(NULL);


    // Create the test tasks
    PrintWithBuf(buf, BUFSIZE, "StartupTask: Creating the application tasks\n");

    // The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h
    OSTaskCreate(Mp3StreamTask, (void *)0, &Mp3StreamTaskStk[APP_CFG_TASK_START_STK_SIZE - 1], APP_TASK_TEST1_PRIO);
    OSTaskCreate(ActivityTask, (void *)0, &ActivityTaskStk[APP_CFG_TASK_START_STK_SIZE - 1], APP_TASK_TEST2_PRIO);
    OSTaskCreate(TouchEventTask, (void *)0, &TouchEventTaskStk[APP_CFG_TASK_START_STK_SIZE - 1], APP_TASK_TEST3_PRIO);

    // Delete ourselves, letting the work be done in the new tasks.
    PrintWithBuf(buf, BUFSIZE, "StartupTask: deleting self\n");
    OSTaskDel(OS_PRIO_SELF);
}

static void renderLcdInitialState() {
    /*  Print a message on the LCD
    char buf[BUFSIZE];
    lcdCtrl.setCursor(40, 60);
    lcdCtrl.setTextColor(ILI9341_WHITE);
    lcdCtrl.setTextSize(2);
    PrintToLcdWithBuf(buf, BUFSIZE, "Hello World!"); */
    lcdCtrl.fillScreen(inactivecolor);
    renderPausePlayCircle(lcdCtrl, activecolor);
    renderPlay(lcdCtrl,activecolor);
    renderSkip(lcdCtrl, activecolor);
}

// Renders a character at the current cursor position on the LCD
static void PrintCharToLcd(char c) { lcdCtrl.write(c); }

/************************************************************************************

   Print a formated string with the given buffer to LCD.
   Each task should use its own buffer to prevent data corruption.

************************************************************************************/
void PrintToLcdWithBuf(char *buf, int size, char *format, ...) {
    va_list args;
    va_start(args, format);
    PrintToDeviceWithBuf(PrintCharToLcd, buf, size, format, args);
    va_end(args);
}

/************************************************************************************

    TouchEventTask: propagate touch events to other contexts

************************************************************************************/

void TouchEventTask(void *pdata) {
    INT8U uCOSerr = OS_ERR_NONE;
    char buf[BUFSIZE];
    PrintWithBuf(buf, BUFSIZE, "TouchEventTask: starting\n");

    PrintWithBuf(buf, BUFSIZE, "Opening I2C1 handle for touch controller\n");
    HANDLE hI2C1 = Open(PJDF_DEVICE_ID_I2C1, 0);
    if (!PJDF_IS_VALID_HANDLE(hI2C1))
        while (1)
            ;
    touchCtrl.setPjdfHandle(hI2C1);

    if (!touchCtrl.begin(40)) { // pass in 'sensitivity' coefficient
        PrintWithBuf(buf, BUFSIZE, "Couldn't start FT6206 touchscreen controller\n");
        while (1)
            ;
    }

    TS_Point rawPoint = tsReleasePseudoPoint;
    while (1) {
        
        rawPoint = *(TS_Point *)OSMboxPend(touchMbox, 0, &uCOSerr);
        while (uCOSerr != OS_ERR_TIMEOUT) {
            // remap point, post point, and if all is well  pend for 5. timeout will indicate touch release event
            // transform raw touch shield coordinates to display coordinates and point
            *pTsPointBuffer = TS_Point(MapTouchToScreen(rawPoint.x, ILI9341_TFTWIDTH), MapTouchToScreen(rawPoint.y, ILI9341_TFTHEIGHT));
            //if (rawPoint.x == 0 && rawPoint.y == 0) continue;
            uCOSerr = OSQPost(activityQueue, (void *)pTsPointBuffer);
            ++pTsPointBuffer;
            if (pTsPointBuffer == pEndTsPointBuffer) pTsPointBuffer = pStartTsPointBuffer;
            rawPoint = *(TS_Point *)OSMboxPend(touchMbox, 5, &uCOSerr);
        }
        // indefinate pend returning, followed by timeout waiting for touch event indicate touch release
        if (uCOSerr == OS_ERR_TIMEOUT && OSQPost(activityQueue, (void *)&tsReleasePseudoPoint) != OS_ERR_NONE) {
              PrintWithBuf(buf, BUFSIZE, "Failed to post touch screen release pseudo point\n");
        }
    }
}

/************************************************************************************

    ActivityTask: pend for events and propagate accordingly, for now
                     it also handles display event implentation

************************************************************************************/

void ActivityTask(void *pdata) {
    INT8U uCOSerr = OS_ERR_NONE;
    PjdfErrCode pjdfErr;
    INT32U length;
    char buf[BUFSIZE];
    PrintWithBuf(buf, BUFSIZE, "ActivityTask: starting\n");

    PrintWithBuf(buf, BUFSIZE, "Opening LCD device handle: %s\n", PJDF_DEVICE_ID_LCD_ILI9341);
    // Open an instance of the LCD driver
    HANDLE hLcd = Open(PJDF_DEVICE_ID_LCD_ILI9341, 0);
    if (!PJDF_IS_VALID_HANDLE(hLcd))
        while (1)
            ;

    PrintWithBuf(buf, BUFSIZE, "Opening LCD SPI device handle: %s\n", LCD_SPI_DEVICE_ID);
    // SPI communication is used for the Lcd. Open an instance of the SPI driver for the LCD driver
    HANDLE hSPI = Open(LCD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI))
        while (1)
            ;

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hLcd, PJDF_CTRL_LCD_SET_SPI_HANDLE, &hSPI, &length);
    if (PJDF_IS_ERROR(pjdfErr))
        while (1)
            ;

    PrintWithBuf(buf, BUFSIZE, "Initializing LCD controller\n");
    lcdCtrl.setPjdfHandle(hLcd);
    lcdCtrl.begin();

    renderLcdInitialState();
    // initialize display state variables
    uint32_t tickTime = 0;
    bool playing = false;
    boolean releasedTouch = false;
    boolean contiguousTouch = false;
    TS_Point p = tsReleasePseudoPoint;
    void *msg = NULL;
    // Pend for activity
    while (1) {
        msg = OSQPend(activityQueue, 0, &uCOSerr);
        if (uCOSerr != OS_ERR_NONE) {
            p = tsReleasePseudoPoint;
            PrintWithBuf(buf, BUFSIZE, "Failed to pend activityQueue\n");
            continue;
        }

        if (msg ==  &tsReleasePseudoPoint && contiguousTouch) { // (0,0) <-> release occurred
          releasedTouch = true;
          contiguousTouch = false;
        } else { // the only activity expected is touch, contiguousTouch occurred
          contiguousTouch = true;
          releasedTouch = false;
          p = *((TS_Point *)msg);

        }
        // update display based on activity and "listeners"
        if (releasedTouch) { // check touch release "listeners"
          tickTime = OSTimeGet();
          if (tickTime - lastActivityTime > maxActivityRateInTicks) {
                lastActivityTime = tickTime;
          } else {
                continue;
          }
          PrintWithBuf(buf, BUFSIZE, "ActivityTask: firing relevant releasedTouch events\n");
          if (touchedPausePlay(p)) {

          // set playing according to state accounting for curtouched
          playing = !playing;
          // now render and send messages to make playing state consistent between contexts
          if (playing) {
            uCOSerr = OSMboxPost(mp3StreamMbox, (void *)&mp3MessagePlay);
            if (uCOSerr == OS_ERR_NONE) {
                PrintWithBuf(buf, BUFSIZE, "Rendered play active and sent play message\n");
                renderPlay(lcdCtrl,inactivecolor);  // hide play action
                renderPause(lcdCtrl,activecolor);  // show pause action
            }
          } else { // if paused
            uCOSerr = OSMboxPost(mp3StreamMbox, (void *)&mp3MessagePause);
            if (uCOSerr == OS_ERR_NONE) {
                PrintWithBuf(buf, BUFSIZE, "Sent pause message w/ no err\n");
                renderPause(lcdCtrl,inactivecolor); // hide pause action
                renderPlay(lcdCtrl,activecolor);  // show play action
            }
          }
          PrintWithBuf(buf, BUFSIZE, "OSMboxPost to mp3 %u\n", uCOSerr);
          if (uCOSerr != OS_ERR_NONE) {
            playing = !playing; // revert state since nothing is rendered on err
          }
          } else if (touchedSkip(p)){
            
            if (OSMboxPost(mp3StreamMbox, (void *)&mp3MessageSkip) == OS_ERR_NONE) {
                              PrintWithBuf(buf, BUFSIZE, "Sent skip message to Mp3StreamTask\n");
            } else {
                                            PrintWithBuf(buf, BUFSIZE, "Failed to send skip message to Mp3StreamTask\n");
            }

          }
        }
        if (contiguousTouch) { // check contiguous touch "listener"
            if (!touchedPausePlay(p) && !touchedSkip(p)) { // draw point outside of pause play only
                lcdCtrl.fillCircle(p.x, p.y, PENRADIUS, ILI9341_RED);
            }
        }
    }
}
/************************************************************************************

   Mp3StreamTask: write mp3 data based on messages

************************************************************************************/

void Mp3StreamTask(void *pdata) {
      INT8U uCOSerr = OS_ERR_NONE;

    PjdfErrCode pjdfErr;
    INT32U length;

    OSTimeDly(2000); // Allow other task to initialize LCD before we use it.

    char buf[BUFSIZE];
    PrintWithBuf(buf, BUFSIZE, "Mp3DemoTask: starting\n");

    PrintWithBuf(buf, BUFSIZE, "Opening MP3 driver: %s\n", PJDF_DEVICE_ID_MP3_VS1053);
    // Open handle to the MP3 decoder driver
    HANDLE hMp3 = Open(PJDF_DEVICE_ID_MP3_VS1053, 0);
    if (!PJDF_IS_VALID_HANDLE(hMp3))
        while (1)
            ;

    PrintWithBuf(buf, BUFSIZE, "Opening MP3 SPI driver: %s\n", MP3_SPI_DEVICE_ID);
    // We talk to the MP3 decoder over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the MP3 driver.
    HANDLE hSPI = Open(MP3_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI))
        while (1)
            ;

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hMp3, PJDF_CTRL_MP3_SET_SPI_HANDLE, &hSPI, &length);
    if (PJDF_IS_ERROR(pjdfErr))
        while (1)
            ;

    // Send initialization data to the MP3 decoder and run a test
    PrintWithBuf(buf, BUFSIZE, "Starting MP3 device test\n");
    Mp3Init(hMp3);
    
    //
    //
        // Initialize SD card
    PrintWithBuf(buf, PRINTBUFMAX, "Opening handle to SD driver: %s\n", PJDF_DEVICE_ID_SD_ADAFRUIT);
    HANDLE hSD = Open(PJDF_DEVICE_ID_SD_ADAFRUIT, 0);
    if (!PJDF_IS_VALID_HANDLE(hSD))
        while (1)
            ;

    PrintWithBuf(buf, PRINTBUFMAX, "Opening SD SPI driver: %s\n", SD_SPI_DEVICE_ID);
    // We talk to the SD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the SD driver.
    hSPI = Open(SD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI))
        while (1)
            ;

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hSD, PJDF_CTRL_SD_SET_SPI_HANDLE, &hSPI, &length);
    if (PJDF_IS_ERROR(pjdfErr))
        while (1)
            ;
   
    StorageInit(hSD);
    File mp3File;
    uint8_t mp3idx = 0;
    uint8_t mp3Count = StorageGetMp3Count();
    if (mp3Count == 0) {
        PrintWithBuf(buf, PRINTBUFMAX, "Failed to find any mp3 files...\n");
        while (1)
         ;

    }
    // Set app status flag mp3StreamReady
    OSFlagPost(AppStatus, appMp3StreamReady, OS_FLAG_SET, &uCOSerr);
    if (uCOSerr != OS_ERR_NONE) {
        PrintWithBuf(buf, PRINTBUFMAX, "Failed to set %s\n", PJDF_DEVICE_ID_SD_ADAFRUIT);
    }
    // Primary routine
    
    while (1) {
      mp3File = StorageGetMp3File(mp3idx);
      if (mp3File.failbit()) {
         PrintWithBuf(buf, BUFSIZE, "Skipping streaming of file @ mp3idx=%u\n", mp3idx);
      } else {
        PrintWithBuf(buf, BUFSIZE, "Begin streaming sound file  mp3idx=%u\n", mp3idx);
        Mp3StreamSDFile(hMp3, mp3File);
        PrintWithBuf(buf, BUFSIZE, "Done streaming sound file  mp3idx=%u\n", mp3idx);
      }
      ++mp3idx;
      mp3idx %= mp3Count;
    }
}


/************************************************************************************

   part of external touch interrupts

************************************************************************************/
void GetTouchPoint()
{
  static TS_Point rawPoint;
  rawPoint = touchCtrl.getPoint();
  OSMboxPost(touchMbox, &rawPoint);
}