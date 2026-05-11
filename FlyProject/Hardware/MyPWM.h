#ifndef __MYPWM_H
#define __MYPWM_H
#include "board.h"

/*
 * 文件目录:
 * 1. MyPWM_Init     初始化电机 PWM 底层定时器和 GPIO。
 * 2. MyPWM_PWM1~4   设置四路 PWM 比较值。
 */

void MyPWM_Init(void);
void MyPWM_PWM1(uint16_t Compare);
void MyPWM_PWM2(uint16_t Compare);
void MyPWM_PWM3(uint16_t Compare);
void MyPWM_PWM4(uint16_t Compare);
#endif
