#include "MyPWM.h"

// 定时器TIM2 的 通道1，通道2 （PA0，PA1）
// 定时器TIM3 的 通道1，通道2（重定向到 PB4，PB5）

void MyPWM_Init(void)
{
    // 开GPIO输出时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // TIM3的引脚重映射
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 禁用JTAG功能
    GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);     // 重定向到 PB4，PB5

    // 开TIM2、TIM3时钟
    RCC_APB1PeriphClockCmd(TIM2_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(TIM3_RCC, ENABLE);

    // 选择TIM2、TIM3的内部时钟
    TIM_InternalClockConfig(TIM2);
    TIM_InternalClockConfig(TIM3);

    // TIM 时基单元初始化
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitTypeStructure;
    TIM_TimeBaseInitTypeStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitTypeStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitTypeStructure.TIM_Period = 1000 - 1; // ARR
    TIM_TimeBaseInitTypeStructure.TIM_Prescaler = 9 - 1; // PSC
    TIM_TimeBaseInitTypeStructure.TIM_RepetitionCounter = 0;
    // 希望输出的频率 是8KHz

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitTypeStructure);
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitTypeStructure);

    // 我选择通道1
    TIM_OCInitTypeDef TIM_OCInitTypeStructure;

    // 不用的参数希望给予默认值，不留空
    TIM_OCStructInit(&TIM_OCInitTypeStructure);

    //	TIM_OCInitTypeStructure.TIM_OCIdleState = ;  //only for TIM1 and TIM8.
    TIM_OCInitTypeStructure.TIM_OCMode = TIM_OCMode_PWM1; // PWM1模式
    //	TIM_OCInitTypeStructure.TIM_OCNIdleState = ; // only for TIM1 and TIM8.
    //	TIM_OCInitTypeStructure.TIM_OCNPolarity = ; // only for TIM1 and TIM8.
    TIM_OCInitTypeStructure.TIM_OCPolarity = TIM_OCPolarity_High; // 极性是否反转
    //	TIM_OCInitTypeStructure.TIM_OutputNState = ; //only for TIM1 and TIM8.
    TIM_OCInitTypeStructure.TIM_OutputState = TIM_OutputState_Enable; // 开启输出
    TIM_OCInitTypeStructure.TIM_Pulse = 0;                            // 初始化的CCR 的值 between 0x0000 and 0xFFFF

    // 初始化通道
    TIM_OC1Init(TIM2, &TIM_OCInitTypeStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitTypeStructure);
    TIM_OC1Init(TIM3, &TIM_OCInitTypeStructure);
    TIM_OC2Init(TIM3, &TIM_OCInitTypeStructure);

    // GPIO输出口配置
    GPIO_InitTypeDef GPIO_InitTypeStructure;
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 模式？ 输出-->  推挽复用输出
    GPIO_InitTypeStructure.GPIO_Pin = TIM2_CH1_Pin | TIM2_CH2_Pin;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TIM2_GPIO, &GPIO_InitTypeStructure);

    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 模式？ 输出-->  推挽复用输出
    GPIO_InitTypeStructure.GPIO_Pin = TIM3_CH1_Pin | TIM3_CH2_Pin;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TIM3_GPIO, &GPIO_InitTypeStructure);

    // 启用
    TIM_Cmd(TIM2, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

// 设置占空比
void MyPWM_PWM1(uint16_t Compare)
{
    TIM_SetCompare1(TIM2, Compare);
}

void MyPWM_PWM2(uint16_t Compare)
{
    TIM_SetCompare2(TIM2, Compare);
}

void MyPWM_PWM3(uint16_t Compare)
{
    TIM_SetCompare1(TIM3, Compare);
}

void MyPWM_PWM4(uint16_t Compare)
{
    TIM_SetCompare2(TIM3, Compare);
}
