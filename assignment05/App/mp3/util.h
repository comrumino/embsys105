/*
    mp3/util.h
    Some utility functions for controlling the MP3 decoder.

    Developed for University of Washington embedded systems programming certificate

    2016/2 Nick Strathy wrote/arranged it
*/

#ifndef __MP3UTIL_H
#define __MP3UTIL_H
#include "SD.h"

const uint32_t mp3MessagePlay = 0x1;
const uint32_t mp3MessagePause = 0x2;
const uint32_t mp3MessageSkip = 0x3;

PjdfErrCode Mp3GetRegister(HANDLE hMp3, INT8U *cmdInDataOut, INT32U bufLen);
void Mp3Init(HANDLE hMp3);
void StorageInit(HANDLE hSD);
File StorageGetMp3File(uint8_t idx);
uint8_t StorageGetMp3Count();
void Mp3Test(HANDLE hMp3);
void Mp3Stream(HANDLE hMp3, INT8U *pBuf, INT32U bufLen);
void Mp3StreamSDFile(HANDLE hMp3, File dataFile);

#endif