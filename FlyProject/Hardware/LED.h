/*
 * 文件名: LED.h
 * 功能说明:
 * 1. SetLEDTypeDef   LED 闪烁配置结构体，包含闪烁间隔和显示模式。
 * 2. setLED          LED 当前配置全局变量声明。
 * 3. LED_Init        初始化 LED GPIO 和默认闪烁状态。
 * 4. LED_Blink       按 setLED.status 执行对应灯效。
 */
#ifndef __LED_H
#define __LED_H

#include "board.h"

typedef struct
{
    uint16_t FlashTime; // 闪烁间隔，单位 ms，由 LED_Blink 中的 vTaskDelay 使用。
    enum
    {
        AlwaysOn,       // 全亮
        AlwaysOff,      // 全灭
        OneByOne,       // 四个灯依次闪烁
        AllFlashLight,  // 四个灯同时闪烁
        AlternateFlash, // 前后或左右交替闪烁
    } status;
} SetLEDTypeDef;

extern SetLEDTypeDef setLED;

void LED_Init(void);
void LED_Blink(void);

#endif
