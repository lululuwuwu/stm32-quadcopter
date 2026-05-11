#include "FlyContrl.h"



uint16_t MOTOR1,MOTOR2, MOTOR3, MOTOR4; // 电机的pwm值

void FlyContrl_Init(void)
{
    MyPWM_Init();
}

// 限制大小的函数
uint32_t Limit(uint32_t value, uint32_t min, uint32_t max)
{
    if (value >= max)
        value = max;
    if (value <= min)
        value = min;

    return value;
}

void FlyContrl_Motor_3ms(void)
{
        // 限制动力范围
        MyPWM_PWM1(Limit(MOTOR1, 0, 100));
        MyPWM_PWM2(Limit(MOTOR2, 0, 100));
        MyPWM_PWM3(Limit(MOTOR3, 0, 100));
        MyPWM_PWM4(Limit(MOTOR4, 0, 100));
        //效果日志
		//Serial3_SendLog("m1:%d,m2:%d,m3:%d,m4:%d\n", MOTOR1, MOTOR2, MOTOR3, MOTOR4);
   
}

