#ifndef __MYADC_H
#define __MYADC_H

#include "stm32f10x.h"

/* Stick pins and peripherals. */
#define STICK_ADC_RCC RCC_APB2Periph_ADC1
#define STICK_DMA_RCC RCC_AHBPeriph_DMA1
#define STICK_RCC RCC_APB2Periph_GPIOA
#define STICK_GPIO GPIOA
#define STICK_THR_PIN GPIO_Pin_1  /* THR: ADC_Channel_1 */
#define STICK_YAW_PIN GPIO_Pin_0  /* YAW: ADC_Channel_0 */
#define STICK_PITH_PIN GPIO_Pin_3 /* PIT: ADC_Channel_3 */
#define STICK_ROLL_PIN GPIO_Pin_2 /* ROL: ADC_Channel_2 */
#define STICK_PITCH_PIN STICK_PITH_PIN

/* Axis order matches StickADCRaw[] DMA buffer order. */
typedef enum
{
    STICK_AXIS_THR = 0,
    STICK_AXIS_YAW,
    STICK_AXIS_PIT,
    STICK_AXIS_ROL,
    STICK_AXIS_COUNT
} StickAxisTypeDef;

void StickADC_Init(void);

/* Read one DMA sample per axis into the moving average window. */
void StickADC_FilterUpdate(void);

uint16_t StickADC_GetRaw(StickAxisTypeDef axis);
uint16_t StickADC_GetFilteredRaw(StickAxisTypeDef axis);

/* Get calibrated RC value, range 1000~2000. */
int16_t StickADC_GetRCValue(StickAxisTypeDef axis);

/* Full calibration: start, move sticks to limits, return to center, finish. */
void StickADC_CalibrationStart(void);
void StickADC_CalibrationSample(void);
uint8_t StickADC_CalibrationFinish(uint8_t save_to_flash);
void StickADC_CalibrationCancel(void);
uint8_t StickADC_IsCalibrationRunning(void);

/* Fast center calibration for YAW/PIT/ROL. */
uint8_t StickADC_CalibrationSetCenter(uint8_t save_to_flash);

#endif
