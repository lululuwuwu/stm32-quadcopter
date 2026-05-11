/*
 * 文件名: FlyContrl.c
 * 功能说明:
 * 1. MOTOR1~4              四路电机 PWM 目标值，供控制任务写入。
 * 2. FlyContrl_Init        初始化飞控执行层依赖的 PWM 外设。
 * 3. Limit                 将数值限制在指定上下限之间。
 * 4. FlyContrl_Motor_3ms   3ms 周期刷新四路电机 PWM 输出。
 *
 * 本文件处在业务控制层和 PWM 驱动层之间：上层只关心电机目标值，本层负责限幅并调用 MyPWM。
 */
#include "FlyContrl.h"

// 四路电机 PWM 目标值。当前单位与 MyPWM 的 CCR 比较值一致，范围暂按 0~100 使用。
uint16_t MOTOR1, MOTOR2, MOTOR3, MOTOR4;

void FlyContrl_Init(void)
{
    // PWM 属于电机执行层的底层资源，先初始化定时器和 GPIO，再允许周期任务刷新输出。
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
    // 当前还没有接入完整姿态控制器，先按安全的小范围输出。
    // 后续如果使用更高 PWM 分辨率，需要同时调整 MyPWM 的 ARR 和这里的限幅。
    MyPWM_PWM1(Limit(MOTOR1, 0, 100));
    MyPWM_PWM2(Limit(MOTOR2, 0, 100));
    MyPWM_PWM3(Limit(MOTOR3, 0, 100));
    MyPWM_PWM4(Limit(MOTOR4, 0, 100));

    // 高频任务中串口日志会影响实时性，需要调试电机输出时再临时打开。
    // Serial3_SendLog("m1:%d,m2:%d,m3:%d,m4:%d\n", MOTOR1, MOTOR2, MOTOR3, MOTOR4);
}
