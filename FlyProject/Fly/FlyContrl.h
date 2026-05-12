/*
 * 文件名: FlyContrl.h
 * 功能说明:
 * 1. MOTOR1~4              四路电机 PWM 目标值声明。
 * 2. FlyContrl_Init        飞控执行层初始化，当前主要初始化 PWM 外设。
 * 3. FlyContrl_Motor_3ms   3ms 电机输出刷新入口。
 *
 * 注意:
 * - MOTOR1~4 是当前电机输出目标值，后续姿态控制器或遥控输入会写入这些变量。
 * - 本模块只负责把目标值限制后输出到底层 PWM，不在这里做姿态解算。
 */
#ifndef __FLYCONTRL_H
#define __FLYCONTRL_H

#include "board.h"

extern uint16_t MOTOR1, MOTOR2, MOTOR3, MOTOR4;

void FlyContrl_Init(void);
void FlyContrl_Unlock_10ms(void);
void FlyContrl_Motor_3ms(void);

#endif
