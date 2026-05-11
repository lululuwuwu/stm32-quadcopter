#include "MyPWM.h"

/*
 * 文件目录:
 * 1. MyPWM_Init  初始化 TIM2/TIM3 四路 PWM 输出。
 * 2. MyPWM_PWM1  设置 TIM2 通道 1 的 PWM 比较值。
 * 3. MyPWM_PWM2  设置 TIM2 通道 2 的 PWM 比较值。
 * 4. MyPWM_PWM3  设置 TIM3 通道 1 的 PWM 比较值。
 * 5. MyPWM_PWM4  设置 TIM3 通道 2 的 PWM 比较值。
 */

// 定时器 TIM2 使用通道 1、通道 2，对应 PA0、PA1。
// 定时器 TIM3 使用通道 1、通道 2，重映射到 PB4、PB5。

void MyPWM_Init(void)
{
    // 打开 GPIOA、GPIOB 的外设时钟。
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // TIM3 通道映射到 PB4/PB5，需要关闭 JTAG 释放 PB4。
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 禁用 JTAG 功能
    GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);     // TIM3 部分重映射到 PB4、PB5

    // 打开 TIM2、TIM3 时钟。
    RCC_APB1PeriphClockCmd(TIM2_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(TIM3_RCC, ENABLE);

    // TIM2、TIM3 使用内部时钟源。
    TIM_InternalClockConfig(TIM2);
    TIM_InternalClockConfig(TIM3);

    // 定时器基础单元初始化：72MHz / 9 / 1000 = 8KHz。
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitTypeStructure;
    TIM_TimeBaseInitTypeStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitTypeStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitTypeStructure.TIM_Period = 1000 - 1; // ARR
    TIM_TimeBaseInitTypeStructure.TIM_Prescaler = 9 - 1; // PSC
    TIM_TimeBaseInitTypeStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitTypeStructure);
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitTypeStructure);

    // 输出比较通道统一配置为 PWM1 模式。
    TIM_OCInitTypeDef TIM_OCInitTypeStructure;

    // 先填入标准库默认值，避免未使用字段保持随机值。
    TIM_OCStructInit(&TIM_OCInitTypeStructure);

    //	TIM_OCInitTypeStructure.TIM_OCIdleState = ;  //only for TIM1 and TIM8.
    TIM_OCInitTypeStructure.TIM_OCMode = TIM_OCMode_PWM1; // PWM1 模式
    //	TIM_OCInitTypeStructure.TIM_OCNIdleState = ; // only for TIM1 and TIM8.
    //	TIM_OCInitTypeStructure.TIM_OCNPolarity = ; // only for TIM1 and TIM8.
    TIM_OCInitTypeStructure.TIM_OCPolarity = TIM_OCPolarity_High; // 高电平为有效极性
    //	TIM_OCInitTypeStructure.TIM_OutputNState = ; //only for TIM1 and TIM8.
    TIM_OCInitTypeStructure.TIM_OutputState = TIM_OutputState_Enable; // 使能输出
    TIM_OCInitTypeStructure.TIM_Pulse = 0;                            // 初始 CCR 值为 0

    // 初始化四路 PWM 输出通道。
    TIM_OC1Init(TIM2, &TIM_OCInitTypeStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitTypeStructure);
    TIM_OC1Init(TIM3, &TIM_OCInitTypeStructure);
    TIM_OC2Init(TIM3, &TIM_OCInitTypeStructure);

    // GPIO 配置为复用推挽输出，交由定时器输出 PWM 波形。
    GPIO_InitTypeDef GPIO_InitTypeStructure;
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitTypeStructure.GPIO_Pin = TIM2_CH1_Pin | TIM2_CH2_Pin;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TIM2_GPIO, &GPIO_InitTypeStructure);

    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitTypeStructure.GPIO_Pin = TIM3_CH1_Pin | TIM3_CH2_Pin;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TIM3_GPIO, &GPIO_InitTypeStructure);

    // 启动定时器后，四路通道开始按 CCR 值输出 PWM。
    TIM_Cmd(TIM2, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

// 设置 1 号电机 PWM 占空比，Compare 范围由上层控制。
void MyPWM_PWM1(uint16_t Compare)
{
    TIM_SetCompare1(TIM2, Compare);
}

// 设置 2 号电机 PWM 占空比，Compare 范围由上层控制。
void MyPWM_PWM2(uint16_t Compare)
{
    TIM_SetCompare2(TIM2, Compare);
}

// 设置 3 号电机 PWM 占空比，Compare 范围由上层控制。
void MyPWM_PWM3(uint16_t Compare)
{
    TIM_SetCompare1(TIM3, Compare);
}

// 设置 4 号电机 PWM 占空比，Compare 范围由上层控制。
void MyPWM_PWM4(uint16_t Compare)
{
    TIM_SetCompare2(TIM3, Compare);
}
