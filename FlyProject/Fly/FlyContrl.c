#include "FlyContrl.h"

/*
 * 文件目录:
 * 1. MOTOR1~4              四路电机 PWM 目标值。
 * 2. FlyContrl_Init        初始化飞控执行层依赖的 PWM 外设。
 * 3. Limit                 将数值限制在指定范围内。
 * 4. FlyContrl_Motor_3ms   3ms 周期刷新四路电机 PWM。
 */

uint16_t MOTOR1,MOTOR2, MOTOR3, MOTOR4; // 四路电机 PWM 目标值

void FlyContrl_Init(void)
{
    MyPWM_Init();
}

// 将输入值限制在 min 和 max 之间，避免越界写入 PWM 比较寄存器。
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
        // 电机输出暂按 0~100 限幅，上层控制算法应保证 MOTOR1~4 的目标值合理。
        MyPWM_PWM1(Limit(MOTOR1, 0, 100));
        MyPWM_PWM2(Limit(MOTOR2, 0, 100));
        MyPWM_PWM3(Limit(MOTOR3, 0, 100));
        MyPWM_PWM4(Limit(MOTOR4, 0, 100));
        // 高频任务中串口日志会影响实时性，需要调试时再打开。
		//Serial3_SendLog("m1:%d,m2:%d,m3:%d,m4:%d\n", MOTOR1, MOTOR2, MOTOR3, MOTOR4);
   
}
