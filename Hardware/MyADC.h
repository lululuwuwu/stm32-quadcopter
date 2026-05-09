#ifndef __MYADC_H
#define __MYADC_H

#include "stm32f10x.h"

/*
 * 摇杆 ADC 外设和 GPIO 定义集中在 board.h。
 * 当前硬件连接关系：
 *   THR 油门：PA1 -> ADC_Channel_1
 *   YAW 偏航：PA0 -> ADC_Channel_0
 *   PIT 俯仰：PA3 -> ADC_Channel_3
 *   ROL 翻滚：PA2 -> ADC_Channel_2
 *   BAT 电量：PB0 -> ADC_Channel_8
 *
 * 注意：这里不再维护具体引脚宏；真正写入 DMA 缓冲区的顺序，
 * 由 MyADC.c 中 ADC_RegularChannelConfig() 的规则通道顺序决定。
 */

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

void StickADC_Init(void);
void StickADC_FilterUpdate(void);
uint16_t StickADC_GetRaw(StickAxisTypeDef axis);
uint16_t StickADC_GetFilteredRaw(StickAxisTypeDef axis);
int16_t StickADC_GetRCValue(StickAxisTypeDef axis);
uint16_t StickADC_GetBatteryMv(void);
void StickADC_CalibrationStart(void);
void StickADC_CalibrationSample(void);
uint8_t StickADC_CalibrationFinish(uint8_t save_to_flash);
void StickADC_CalibrationCancel(void);
uint8_t StickADC_IsCalibrationRunning(void);
uint8_t StickADC_CalibrationSetCenter(uint8_t save_to_flash);

#endif
