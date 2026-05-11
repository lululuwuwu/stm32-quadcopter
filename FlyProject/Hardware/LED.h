#ifndef __LED_H
#define __LED_H
#include "board.h"

typedef struct
{
    uint16_t FlashTime; // 闪烁间隔时间，毫秒
    enum
    {
        AlwaysOn,       // 全亮
        AlwaysOff,      // 全灭
        OneByOne,       // 一个接一个的逆时针闪烁
        AllFlashLight,  // 全部同时闪烁
        AlternateFlash, // 交替闪烁
    } status;
} SetLEDTypeDef;

extern SetLEDTypeDef setLED;

void LED_Init(void);

void LED_Blink(void);

#endif
