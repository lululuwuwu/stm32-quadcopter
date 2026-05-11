/*
 * 文件名: MyPWM.h
 * 功能说明:
 * 1. MyPWM_Init     初始化 TIM2/TIM3、GPIO 和四路 PWM 输出通道。
 * 2. MyPWM_PWM1     设置 TIM2_CH1 的比较值，对应 1 号电机。
 * 3. MyPWM_PWM2     设置 TIM2_CH2 的比较值，对应 2 号电机。
 * 4. MyPWM_PWM3     设置 TIM3_CH1 的比较值，对应 3 号电机。
 * 5. MyPWM_PWM4     设置 TIM3_CH2 的比较值，对应 4 号电机。
 */
#ifndef __MYPWM_H
#define __MYPWM_H

#include "board.h"

void MyPWM_Init(void);
void MyPWM_PWM1(uint16_t Compare);
void MyPWM_PWM2(uint16_t Compare);
void MyPWM_PWM3(uint16_t Compare);
void MyPWM_PWM4(uint16_t Compare);

#endif
