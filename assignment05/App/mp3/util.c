/*
    mp3/util.c
    Some utility functions for controlling the MP3 decoder.

    Developed for University of Washington embedded systems programming certificate

    2016/2 Nick Strathy wrote/arranged it
*/

#include "util.h"
#include "SD.h"
#include "bsp.h"
#include "print.h"

#include <Adafruit_FT6206.h>
#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_ILI9341.h>
void delay(uint32_t time);

extern OS_FLAG_GRP *AppStatus;
extern OS_EVENT *mp3StreamMbox;
static File root;
// mp3 info
static struct Mp3Files {
    const size_t max = 6;
    File arr[6 + 1];
    uint8_t size = 0;
} mp3Files;

extern BOOLEAN nextSong;

static void Mp3StreamInit(HANDLE hMp3) {
    INT32U length;

    // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    // Reset the device
    length = BspMp3SoftResetLen;
    Write(hMp3, (void *)BspMp3SoftReset, &length);

    // Set volume
    length = BspMp3SetVol1010Len;
    Write(hMp3, (void *)BspMp3SetVol1010, &length);

    // To allow streaming data, set the decoder mode to Play Mode
    length = BspMp3PlayModeLen;
    Write(hMp3, (void *)BspMp3PlayMode, &length);

    // Set MP3 driver to data mode (subsequent writes will be sent to decoder's data interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_DATA, 0, 0);
}

// Mp3StreamSDFile
// Streams the given file from the SD card to the given MP3 decoder.
// hMP3: an open handle to the MP3 decoder
// pFilename: The file on the SD card to stream.
void Mp3StreamSDFile(HANDLE hMp3, File sdfile) {
    INT8U uCOSerr = OS_ERR_NONE;
    INT32U length;
    char printBuf[PRINTBUFMAX];
    if (sdfile.failbit()) {
        PrintWithBuf(printBuf, PRINTBUFMAX, "Skipping mp3File since failbit is set...\n");
        return;
    }
    Mp3StreamInit(hMp3);

    if (!sdfile) {
        PrintWithBuf(printBuf, PRINTBUFMAX, "Error: could not open SD card file '%s'\n", sdfile.name());
        return;
    }

    INT8U mp3Buf[MP3_DECODER_BUF_SIZE];
    INT32U iBufPos = 0;
    nextSong = OS_FALSE;
    uint32_t mp3Message = 0;
    while (sdfile.available()) {
        // check if there is a message to start
        mp3Message = *((uint32_t *)OSMboxAccept(mp3StreamMbox));
        if (mp3Message == mp3MessagePause) {
            while (mp3Message == mp3MessagePause) {
                mp3Message = *((uint32_t *)OSMboxPend(mp3StreamMbox, 0, &uCOSerr));
            }
        }
        if (mp3Message == mp3MessageSkip) {
            break;
        }
        // write data for mp3
        iBufPos = 0;
        while (sdfile.available() && iBufPos < MP3_DECODER_BUF_SIZE) {

            mp3Buf[iBufPos] = sdfile.read();
            iBufPos++;
        }
        Write(hMp3, mp3Buf, &iBufPos);
    }
    sdfile.seek(0);
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);
    length = BspMp3SoftResetLen;
    Write(hMp3, (void *)BspMp3SoftReset, &length);
}

// Mp3Stream
// Streams the given buffer of MP3 data to the given MP3 decoder
// hMp3: an open handle to the MP3 decoder
// pBuf: MP3 data to stream to the decoder
// bufLen: number of bytes of MP3 data to stream
void Mp3Stream(HANDLE hMp3, INT8U *pBuf, INT32U bufLen) {
    INT8U *bufPos = pBuf;
    INT32U iBufPos = 0;
    INT32U length;
    INT32U chunkLen;
    BOOLEAN done = OS_FALSE;

    Mp3StreamInit(hMp3);

    chunkLen = MP3_DECODER_BUF_SIZE;

    while (!done) {
        // detect last chunk of pBuf
        if (bufLen - iBufPos < MP3_DECODER_BUF_SIZE) {
            chunkLen = bufLen - iBufPos;
            done = OS_TRUE;
        }

        Write(hMp3, bufPos, &chunkLen);

        bufPos += chunkLen;
        iBufPos += chunkLen;
    }

    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);
    length = BspMp3SoftResetLen;
    Write(hMp3, (void *)BspMp3SoftReset, &length);
}

/*
  Listfiles

 This example shows how print out the files in a
 directory on a SD card

 The circuit:
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4

 created   Nov 2010
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
 modified 2 Feb 2014
 by Scott Fitzgerald

 This example code is in the public domain.

 */

void walkDirectoryForMP3(File dir, int numTabs) {
    char printBuf[PRINTBUFMAX];
    while (mp3Files.size < mp3Files.max) { // while size is less than max keep walking for more mp3 files
        File entry = dir.openNextFile();
        // Break when there are no more files
        if (!entry)
            break;
        // Print indententation according to numTabs
        for (uint8_t i = 0; i < numTabs; i++) {
            PrintWithBuf(printBuf, PRINTBUFMAX, "\t");
        }
        // Print tab entry name and availability
        PrintWithBuf(printBuf, PRINTBUFMAX, "%s (failbit %x, dir %u, size %u, mp3 %u, ext %s)\r\n", entry.name(),
                     entry.failbit(), entry.isDirectory(), entry.size(), entry.isMp3(), entry.ext());
        // Branch based on file type
        if (entry.isDirectory()) {
            walkDirectoryForMP3(entry, numTabs + 1);
        } else if (entry.isMp3()) {
            mp3Files.arr[mp3Files.size] = entry;
            mp3Files.size++;
        } else {
            entry.close();
        }
    }
}

// Mp3Init
// Send commands to the MP3 device to initialize it.s
void Mp3Init(HANDLE hMp3) {
    INT32U length;

    if (!PJDF_IS_VALID_HANDLE(hMp3))
        while (1)
            ;

    // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    length = BspMp3SetClockFLen;
    Write(hMp3, (void *)BspMp3SetClockF, &length);

    length = BspMp3SetVol1010Len;
    Write(hMp3, (void *)BspMp3SetVol1010, &length);

    length = BspMp3SoftResetLen;
    Write(hMp3, (void *)BspMp3SoftReset, &length);
}

void StorageInit(HANDLE hSD) {
    // Initialize SD card
    SD.begin(hSD);
    root = SD.open("/");
    walkDirectoryForMP3(root, 0);
}
uint8_t StorageGetMp3Count() { return mp3Files.size; }
File StorageGetMp3File(uint8_t idx) {
    if (idx <= mp3Files.max) {
        return mp3Files.arr[idx];
    } else {
        return File();
    }
}
// Mp3GetRegister
// Gets a register value from the MP3 decoder.
// hMp3: an open handle to the MP3 decoder
// cmdInDataOut: on entry this buffer must contain the
//     command to get the desired register from the decoder. On exit
//     this buffer will be OVERWRITTEN by the data output by the decoder in
//     response to the command. Note: the buffer must not reside in readonly memory.
// bufLen: the number of bytes in cmdInDataOut.
// Returns: PJDF_ERR_NONE if no error otherwise an error code.
PjdfErrCode Mp3GetRegister(HANDLE hMp3, INT8U *cmdInDataOut, INT32U bufLen) {
    PjdfErrCode retval;
    if (!PJDF_IS_VALID_HANDLE(hMp3))
        while (1)
            ;

    // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    retval = Read(hMp3, cmdInDataOut, &bufLen);
    return retval;
}

// Mp3Test
// Runs sine wave sound test on the MP3 decoder.
// For VS1053, the sine wave test only works if run immediately after a hard
// reset of the chip.
void Mp3Test(HANDLE hMp3) {
    INT32U length;

    if (!PJDF_IS_VALID_HANDLE(hMp3))
        while (1)
            ;

    // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    // Make sure we can communicate with the device by sending a command to read a register value

    // First set volume to a known value
    length = BspMp3SetVol1010Len;
    Write(hMp3, (void *)BspMp3SetVol1010, &length);

    // Now get the volume setting on the device
    INT8U buf[10];
    memcpy(buf, BspMp3ReadVol, BspMp3ReadVolLen); // copy command from flash to a ram buffer
    Mp3GetRegister(hMp3, buf, BspMp3ReadVolLen);
    if (buf[2] != 0x10 || buf[3] != 0x10) {
        while (1)
            ; // failed to get data back from the device
    }

    for (int i = 0; i < 10; i++) {
        // set louder volume if i is odd
        if (i & 1) {
            length = BspMp3SetVol1010Len;
            Write(hMp3, (void *)BspMp3SetVol1010, &length);
        } else {
            length = BspMp3SetVol6060Len;
            Write(hMp3, (void *)BspMp3SetVol6060, &length);
        }

        // Put MP3 decoder in test mode
        length = BspMp3TestModeLen;
        Write(hMp3, (void *)BspMp3TestMode, &length);

        // Play a sine wave
        length = BspMp3SineWaveLen;
        Write(hMp3, (void *)BspMp3SineWave, &length);
        Write(hMp3, (void *)BspMp3SineWave, &length); // Sending once sometimes doesn't work!

        OSTimeDly(500);

        // Stop playing the sine wave
        length = BspMp3DeactLen;
        Write(hMp3, (void *)BspMp3Deact, &length);

        OSTimeDly(500);
    }
}
