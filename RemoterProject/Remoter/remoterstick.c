#include "board.h"
#include "MyADC.h"
#include "remoterData.h"
#include "remoterstick.h"

#define REMOTER_RC_MIN 1000
#define REMOTER_RC_MAX 2000
#define REMOTER_RC_LOW_POWER_MV 3500

/*
 * 遥控任务层的完整校准状态。
 * MyADC.c 内部也有 StickADCCalibrating, 用于控制 ADC 校准采样；
 * 这里保留一个状态, 用来判断右键长按是进入校准还是结束校准。
 */
static uint8_t RemoterStickCalibrating;

/* 把最终遥控量限制在 1000~2000, 防止微调或异常映射后越界。 */
static int16_t RemoterLimitRC(int16_t value)
{
    if (value < REMOTER_RC_MIN)
    {
        return REMOTER_RC_MIN;
    }

    if (value > REMOTER_RC_MAX)
    {
        return REMOTER_RC_MAX;
    }

    return value;
}

void RemoterStick_ResetCalibrationState(void)
{
    RemoterStickCalibrating = 0;
}

/*
 * 更新摇杆和遥控器电量数据。
 * 1. 更新 ADC 滑动窗口滤波；
 * 2. 如果正在完整校准, 记录当前 min/max；
 * 3. 把摇杆 ADC 值映射为 1000~2000；
 * 4. 读取 PB0 上的遥控器电池电压并判断低电量。
 */
void RemoterStick_Update(void)
{
    StickADC_FilterUpdate();

    if (StickADC_IsCalibrationRunning())
    {
        StickADC_CalibrationSample();
    }

    RemoterData.THR = StickADC_GetRCValue(STICK_AXIS_THR);

    RemoterData.YAW = RemoterLimitRC(StickADC_GetRCValue(STICK_AXIS_YAW) + RemoterData.OffSet_Yaw);
    RemoterData.PIT = RemoterLimitRC(StickADC_GetRCValue(STICK_AXIS_PIT) + RemoterData.OffSet_Pit);
    RemoterData.ROL = RemoterLimitRC(StickADC_GetRCValue(STICK_AXIS_ROL) + RemoterData.OffSet_Rol);

    RemoterData.Battery_RC = StickADC_GetBatteryMv();
    RemoterData.RC_low_power = RemoterData.Battery_RC < REMOTER_RC_LOW_POWER_MV;
}

uint8_t RemoterStick_IsCalibrating(void)
{
    return RemoterStickCalibrating;
}

/*
 * 第一次右键长按进入完整校准, 开始记录四个轴的 min/max。
 */
void RemoterStick_StartFullCalibration(void)
{
    if (RemoterStickCalibrating)
    {
        return;
    }

    StickADC_CalibrationStart();
    RemoterStickCalibrating = 1;
}

/*
 * 第二次右键长按结束完整校准, 把当前值作为 center 并保存到 Flash。
 */
uint8_t RemoterStick_FinishFullCalibration(void)
{
    if (!RemoterStickCalibrating)
    {
        return 0;
    }

    RemoterStickCalibrating = 0;
    if (StickADC_CalibrationFinish(1))
    {
        RemoterStick_Update();
        return 1;
    }

    return 0;
}

void RemoterStick_CancelCalibration(void)
{
    StickADC_CalibrationCancel();
    RemoterStickCalibrating = 0;
}

/*
 * 快速中点校准。
 * 只校准 YAW/PIT/ROL 三个自动回中轴的 center。
 * 使用前应松开对应摇杆, 让它们自然回中。
 */
uint8_t RemoterStick_CalibrateCenter(void)
{
    uint8_t result;

    result = StickADC_CalibrationSetCenter(1);
    RemoterStick_Update();
    return result;
}
