#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

#define BEEP_RCC RCC_APB2Periph_GPIOB
#define BEEP_GPIO GPIOB
#define BEEP_PIN GPIO_Pin_10

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);

#endif
