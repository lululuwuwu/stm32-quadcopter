/*
 * 文件名: MyPWM.c
 * 功能说明:
 * 1. MyPWM_Init     初始化 TIM2/TIM3 四路 PWM 输出，当前 PWM 频率约为 8KHz。
 * 2. MyPWM_PWM1     设置 TIM2 通道 1 的 CCR，用于 1 号电机输出。
 * 3. MyPWM_PWM2     设置 TIM2 通道 2 的 CCR，用于 2 号电机输出。
 * 4. MyPWM_PWM3     设置 TIM3 通道 1 的 CCR，用于 3 号电机输出。
 * 5. MyPWM_PWM4     设置 TIM3 通道 2 的 CCR，用于 4 号电机输出。
 *
 * 硬件连接:
 * - TIM2_CH1 -> PA0，TIM2_CH2 -> PA1。
 * - TIM3_CH1 -> PB4，TIM3_CH2 -> PB5，使用 TIM3 部分重映射。
 *
 * 注意:
 * - PB4 默认是 JTAG 引脚，初始化中会关闭 JTAG 释放 PB4；SWD 调试仍可使用。
 * - 比较值 Compare 由上层限幅，底层函数只写入对应 CCR。
 */
#include "MyPWM.h"

void MyPWM_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitTypeStructure;
    TIM_OCInitTypeDef TIM_OCInitTypeStructure;
    GPIO_InitTypeDef GPIO_InitTypeStructure;

    // 1. 打开 GPIOA/GPIOB 时钟，PA0/PA1/PB4/PB5 都需要配置为复用推挽输出。
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 2. TIM3 使用 PB4/PB5 输出 PWM，需要开启 AFIO 并做部分重映射。
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 禁用 JTAG，释放 PB3/PB4；保留 SWD。
    GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);     // TIM3_CH1/CH2 重映射到 PB4/PB5。

    // 3. 打开 TIM2/TIM3 时钟，并选择内部时钟源。
    RCC_APB1PeriphClockCmd(TIM2_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(TIM3_RCC, ENABLE);
    TIM_InternalClockConfig(TIM2);
    TIM_InternalClockConfig(TIM3);

    // 4. 配置定时器时基：72MHz / 9 / 1000 = 8KHz。
    //    ARR=999，PSC=8，因此 CCR 取值 0~999 对应 0%~约 100% 占空比。
    TIM_TimeBaseInitTypeStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitTypeStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitTypeStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseInitTypeStructure.TIM_Prescaler = 9 - 1;
    TIM_TimeBaseInitTypeStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitTypeStructure);
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitTypeStructure);

    // 5. 配置输出比较通道为 PWM1 模式，高电平有效，初始占空比为 0。
    TIM_OCStructInit(&TIM_OCInitTypeStructure);
    TIM_OCInitTypeStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitTypeStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitTypeStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitTypeStructure.TIM_Pulse = 0;

    TIM_OC1Init(TIM2, &TIM_OCInitTypeStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitTypeStructure);
    TIM_OC1Init(TIM3, &TIM_OCInitTypeStructure);
    TIM_OC2Init(TIM3, &TIM_OCInitTypeStructure);

    // 6. PWM 引脚必须配置为复用推挽输出，否则定时器通道无法把波形输出到引脚。
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitTypeStructure.GPIO_Pin = TIM2_CH1_Pin | TIM2_CH2_Pin;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TIM2_GPIO, &GPIO_InitTypeStructure);

    GPIO_InitTypeStructure.GPIO_Pin = TIM3_CH1_Pin | TIM3_CH2_Pin;
    GPIO_Init(TIM3_GPIO, &GPIO_InitTypeStructure);

    // 7. 最后启动定时器，四路 PWM 开始按各自 CCR 值输出。
    TIM_Cmd(TIM2, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

void MyPWM_PWM1(uint16_t Compare)
{
    // TIM2_CH1 -> PA0 -> 1 号电机。
    TIM_SetCompare1(TIM2, Compare);
}

void MyPWM_PWM2(uint16_t Compare)
{
    // TIM2_CH2 -> PA1 -> 2 号电机。
    TIM_SetCompare2(TIM2, Compare);
}

void MyPWM_PWM3(uint16_t Compare)
{
    // TIM3_CH1 -> PB4 -> 3 号电机。
    TIM_SetCompare1(TIM3, Compare);
}

void MyPWM_PWM4(uint16_t Compare)
{
    // TIM3_CH2 -> PB5 -> 4 号电机。
    TIM_SetCompare2(TIM3, Compare);
}
