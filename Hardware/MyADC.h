#ifndef __MYADC_H
#define __MYADC_H

#include "stm32f10x.h"

// 摇杆引脚和外设定义
#define STICK_ADC_RCC RCC_APB2Periph_ADC1
#define STICK_DMA_RCC RCC_AHBPeriph_DMA1
#define STICK_RCC RCC_APB2Periph_GPIOA
#define STICK_GPIO GPIOA
#define STICK_THR_PIN GPIO_Pin_1  // 油门：ADC_Channel_1
#define STICK_YAW_PIN GPIO_Pin_0  // 偏航：ADC_Channel_0
#define STICK_PITH_PIN GPIO_Pin_3 // 俯仰：ADC_Channel_3
#define STICK_ROLL_PIN GPIO_Pin_2 // 翻滚：ADC_Channel_2
#define STICK_PITCH_PIN STICK_PITH_PIN

// 摇杆轴编号，顺序和 DMA 缓冲区 StickADCRaw[] 的顺序一致。
typedef enum
{
    STICK_AXIS_THR = 0,
    STICK_AXIS_YAW,
    STICK_AXIS_PIT,
    STICK_AXIS_ROL,
    STICK_AXIS_COUNT
} StickAxisTypeDef;

// 初始化 ADC, 当前用于摇杆 ADC1 + DMA1_Channel1 循环采样。
void MyADC_Init(void);

// 获取 PA0/YAW 原始数字值, 范围 0~4095。
uint16_t MyADC_GetDataValue(void);

// 获取 PA0/YAW 模拟电压值, 返回 mv(毫伏), 按 3.3V 参考估算。
uint16_t MyADC_GetAnalogValue(void);

// 摇杆 ADC 初始化, 使用 ADC1 + DMA1_Channel1 循环采样 PA0~PA3。
void StickADC_Init(void);

// 获取指定摇杆轴原始 ADC 值, 范围 0~4095。
uint16_t StickADC_GetRaw(StickAxisTypeDef axis);

// 获取指定摇杆轴遥控量, 范围 1000~2000。
int16_t StickADC_GetRCValue(StickAxisTypeDef axis);

#endif
