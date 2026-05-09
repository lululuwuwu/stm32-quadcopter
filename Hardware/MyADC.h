#ifndef __MYADC_H
#define __MYADC_H

#include "stm32f10x.h"

/*
 * 摇杆 ADC 外设和 GPIO 定义。
 *
 * 当前硬件连接关系：
 *   THR 油门：PA1 -> ADC_Channel_1
 *   YAW 偏航：PA0 -> ADC_Channel_0
 *   PIT 俯仰：PA3 -> ADC_Channel_3
 *   ROL 翻滚：PA2 -> ADC_Channel_2
 *   BAT 电量：PB0 -> ADC_Channel_8
 *
 * 注意：这里的引脚定义只描述硬件引脚；真正写入 DMA 缓冲区的顺序，
 * 由 MyADC.c 中 ADC_RegularChannelConfig() 的规则通道顺序决定。
 */
#define STICK_ADC_RCC RCC_APB2Periph_ADC1
#define STICK_DMA_RCC RCC_AHBPeriph_DMA1
#define STICK_RCC RCC_APB2Periph_GPIOA
#define STICK_GPIO GPIOA
#define STICK_THR_PIN GPIO_Pin_1  /* 油门：ADC_Channel_1 */
#define STICK_YAW_PIN GPIO_Pin_0  /* 偏航：ADC_Channel_0 */
#define STICK_PITH_PIN GPIO_Pin_3 /* 俯仰：ADC_Channel_3 */
#define STICK_ROLL_PIN GPIO_Pin_2 /* 翻滚：ADC_Channel_2 */
#define STICK_PITCH_PIN STICK_PITH_PIN

#define RC_BAT_RCC RCC_APB2Periph_GPIOB
#define RC_BAT_GPIO GPIOB
#define RC_BAT_PIN GPIO_Pin_0 /* 遥控器电量检测：ADC_Channel_8 */

/*
 * 遥控器电池分压比例。
 *
 * 如果硬件是两个相同电阻分压，PB0 上的电压 = 电池电压 / 2，
 * 因此这里默认乘以 2。若你的电阻比例不同，只需要改这两个宏：
 *   电池电压 = ADC引脚电压 * RC_BAT_DIVIDER_NUM / RC_BAT_DIVIDER_DEN
 */
#define RC_BAT_DIVIDER_NUM 2U
#define RC_BAT_DIVIDER_DEN 1U

/*
 * 摇杆轴编号。
 *
 * 这个枚举值会作为数组下标使用，必须和 MyADC.c 中 StickADCRaw[]、
 * StickADCFiltered[]、StickADCCalibration[] 等数组的顺序保持一致。
 */
typedef enum
{
    STICK_AXIS_THR = 0, /* 油门轴：非自动回中，按最低值~最高值线性映射 */
    STICK_AXIS_YAW,    /* 偏航轴：自动回中，按最低值/中点/最高值分段映射 */
    STICK_AXIS_PIT,    /* 俯仰轴：自动回中，按最低值/中点/最高值分段映射 */
    STICK_AXIS_ROL,    /* 翻滚轴：自动回中，按最低值/中点/最高值分段映射 */
    STICK_AXIS_COUNT
} StickAxisTypeDef;

/*
 * 初始化摇杆 ADC 采集模块。
 *
 * 内部会完成：
 *   1. GPIOA 模拟输入配置；
 *   2. ADC1 扫描转换配置；
 *   3. DMA1_Channel1 循环搬运配置；
 *   4. ADC 自校准；
 *   5. 从 Flash 读取摇杆校准参数；
 *   6. 清空滑动窗口滤波缓存；
 *   7. 启动 ADC + DMA 连续采样。
 *
 * 该函数只需要在摇杆扫描任务启动时调用一次。
 */
void StickADC_Init(void);

/*
 * 更新一次滑动窗口滤波。
 *
 * ADC + DMA 会在后台不断刷新原始值，本函数负责把当前最新的原始 ADC 值
 * 放入滑动窗口，并重新计算每个轴的平均值。
 *
 * 建议由周期任务调用，例如当前工程中每 20ms 调用一次。
 */
void StickADC_FilterUpdate(void);

/* 获取指定轴的 DMA 原始 ADC 值，范围通常为 0~4095。 */
uint16_t StickADC_GetRaw(StickAxisTypeDef axis);

/*
 * 获取指定轴的滑动窗口滤波 ADC 值。
 *
 * 如果窗口还没有任何有效样本，会退回读取当前 DMA 原始值；
 * 一旦 StickADC_FilterUpdate() 被调用过，就返回平滑后的平均值。
 */
uint16_t StickADC_GetFilteredRaw(StickAxisTypeDef axis);

/*
 * 获取校准后的遥控通道值，范围为 1000~2000。
 *
 * THR 油门：
 *   校准最低值 -> 1000
 *   校准最高值 -> 2000
 *
 * YAW/PIT/ROL：
 *   校准最低值 -> 1000
 *   校准中点   -> 1500
 *   校准最高值 -> 2000
 */
int16_t StickADC_GetRCValue(StickAxisTypeDef axis);

/* 获取遥控器电池电压，单位 mV。 */
uint16_t StickADC_GetBatteryMv(void);

/*
 * 开始完整校准。
 *
 * 调用后模块进入校准状态，后续周期性调用 StickADC_CalibrationSample()
 * 会持续记录每个轴出现过的最小 ADC 值和最大 ADC 值。
 *
 * 使用方式：
 *   1. 调用 StickADC_CalibrationStart()；
 *   2. 用户把各个摇杆推到极限位置；
 *   3. 用户把回中轴松回中点，油门放在任意位置也可以；
 *   4. 调用 StickADC_CalibrationFinish(1) 保存。
 */
void StickADC_CalibrationStart(void);

/* 校准过程中采样一次，用于刷新每个轴的 min/max。 */
void StickADC_CalibrationSample(void);

/*
 * 结束完整校准。
 *
 * save_to_flash:
 *   0：只更新 RAM 中的校准值，掉电丢失；
 *   1：校准成功后写入 Flash，下次上电自动读取。
 *
 * 返回值：
 *   0：失败，校准值非法或 Flash 写入失败，会恢复到校准前的参数；
 *   1：成功。
 */
uint8_t StickADC_CalibrationFinish(uint8_t save_to_flash);

/* 取消正在进行的完整校准，并恢复到校准开始前的参数。 */
void StickADC_CalibrationCancel(void);

/* 查询当前是否处于完整校准状态。 */
uint8_t StickADC_IsCalibrationRunning(void);

/*
 * 快速中点校准。
 *
 * 只更新 YAW/PIT/ROL 三个自动回中轴的 center 值；
 * 不更新 THR 油门轴，因为油门不是自动回中，不能用当前值作为中点。
 */
uint8_t StickADC_CalibrationSetCenter(uint8_t save_to_flash);

#endif
