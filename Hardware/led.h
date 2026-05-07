#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

#define LED_RED_RCC RCC_APB2Periph_GPIOB
#define LED_RED_GPIO GPIOB
#define LED_RED_PIN GPIO_Pin_9
#define LED_BLUE_RCC RCC_APB2Periph_GPIOB
#define LED_BLUE_GPIO GPIOB
#define LED_BLUE_PIN GPIO_Pin_1

void LED_Init(void);

void LED_RedOn(void);
void LED_RedOff(void);
void LED_BlueOn(void);
void LED_BlueOff(void);
void LED_RedToggle(void);
void LED_BlueToggle(void);

void LED1_On(void);
void LED1_Off(void);
void LED2_On(void);
void LED2_Off(void);
void LED2_Toggle(void);

#endif
