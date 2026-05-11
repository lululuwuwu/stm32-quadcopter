#ifndef __FLYCONTRL_H
#define __FLYCONTRL_H
#include "board.h"



extern uint16_t MOTOR1, MOTOR2, MOTOR3, MOTOR4; // 电机的pwm值



void FlyContrl_Init(void);
void FlyContrl_Motor_3ms(void);



#endif
