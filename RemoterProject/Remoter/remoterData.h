#ifndef __REMOTERDATA_H
#define __REMOTERDATA_H

#include "stm32f10x.h"

typedef struct
{
    u8 windows;        // 第几个窗口（0,1,2,3）
    u8 NRF_Connect;    // NRFL01 连接: 1成功 0失败
    u8 NRF_Channel;    // 信道号（1-125）
    u8 NRF_RSSI_count; // 信号强度值(0--250)
    u8 RC_low_power;   // 遥控器低电量
    u8 Fly_low_power;  // 飞行器低电量
    u16 Battery_RC;    // 遥控电量 (毫伏)
    u16 Battery_Fly;   // 飞控电量 (毫伏)

    int X, Y, Z, H; // 俯仰（度），翻滚（度），偏航（度），高度(厘米)

    u16 fly_test_flag; // 飞机传感器是否正常可用测试

    /**
     *  BIT0:MPU 5060
     *  BIT1:气压
     *  BIT2:无线模块
     *  BIT3：光流模块
     */

    int16_t THR; // 遥杆--油门 （1000 ~ 2000）
    int16_t YAW; // 遥杆--偏航（1000 ~ 2000）
    int16_t PIT; // 遥杆--俯仰（1000 ~ 2000）
    int16_t ROL; // 遥杆--翻滚（1000 ~ 2000）

    int16_t OffSet_Pit; // 微调俯仰
    int16_t OffSet_Rol; // 微调翻滚
    int16_t OffSet_Yaw; // 微调偏航角

} RemoterDataTypeDef;

extern RemoterDataTypeDef RemoterData;

#endif
