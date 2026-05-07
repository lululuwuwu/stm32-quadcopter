#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

#define Key_RCC RCC_APB2Periph_GPIOB
#define Key_GPIO GPIOB
#define Key_Left GPIO_Pin_8
#define Key_Right GPIO_Pin_7
#define Key_OffSet_Font GPIO_Pin_3
#define Key_OffSet_Back GPIO_Pin_5
#define Key_OffSet_Left GPIO_Pin_4
#define Key_OffSet_Right GPIO_Pin_6

typedef enum
{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_RIGHT_SHORT,
    KEY_EVENT_RIGHT_LONG,
    KEY_EVENT_LEFT_LONG,
    KEY_EVENT_OFFSET_UP_SHORT,
    KEY_EVENT_OFFSET_DOWN_SHORT,
    KEY_EVENT_OFFSET_LEFT_SHORT,
    KEY_EVENT_OFFSET_RIGHT_SHORT
} KeyEventTypeDef;

void Key_Init(void);
KeyEventTypeDef Key_ScanEvent(void);

uint8_t Key1_Pressed(void);
uint8_t Key2_Pressed(void);

#endif
