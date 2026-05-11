#ifndef __FLYCONTRL_H
#define __FLYCONTRL_H
#include "board.h"

/*
 * 文件目录:
 * 1. MOTOR1~4              四路电机 PWM 目标值声明。
 * 2. FlyContrl_Init        飞控执行层初始化。
 * 3. FlyContrl_Motor_3ms   3ms 电机输出刷新入口。
 */

extern uint16_t MOTOR1, MOTOR2, MOTOR3, MOTOR4; // 四路电机 PWM 目标值

void FlyContrl_Init(void);
void FlyContrl_Motor_3ms(void);

#endif
