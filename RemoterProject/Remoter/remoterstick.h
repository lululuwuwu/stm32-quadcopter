#ifndef __REMOTERSTICK_H
#define __REMOTERSTICK_H

#include "stm32f10x.h"

void RemoterStick_ResetCalibrationState(void);
void RemoterStick_Update(void);

uint8_t RemoterStick_IsCalibrating(void);
void RemoterStick_StartFullCalibration(void);
uint8_t RemoterStick_FinishFullCalibration(void);
void RemoterStick_CancelCalibration(void);
uint8_t RemoterStick_CalibrateCenter(void);

#endif
