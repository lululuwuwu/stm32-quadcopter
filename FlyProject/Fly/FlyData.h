/*
 * 文件名: FlyData.h
 * 功能说明:
 * 1. _st_Angle          飞机姿态欧拉角结构体，保存 Pitch/Roll/Yaw。
 * 2. _st_RemoterControl 飞控端接收到的遥控器通道和链路状态。
 * 3. FlyAngle/FlyRemoter 当前飞机姿态和遥控链路全局变量声明。
 *
 * 本文件用于集中声明飞控业务层共享数据。遥控通道由 NRF24L01 任务更新，控制任务读取时
 * 要注意后续如果做复杂结构更新，需要再增加队列或临界区保护。
 */
#ifndef __FLYDATA_H
#define __FLYDATA_H

#include "board.h"

typedef struct
{
    float Pitch; /* 欧拉角中的俯仰角，单位：度 */
    float Roll;  /* 欧拉角中的横滚角，单位：度 */
    float Yaw;   /* 欧拉角中的偏航角，单位：度；当前由陀螺仪积分得到，会随时间漂移 */
} _st_Angle;

typedef struct
{
    uint8_t Connected;     /* 是否收到有效遥控数据包 */
    uint8_t Windows;       /* 遥控器当前窗口号，飞控端仅缓存用于调试 */
    uint8_t NRF_Channel;   /* 当前通信信道 */
    uint8_t RC_low_power;  /* 遥控器低电量标志 */

    int16_t THR;           /* 油门通道，安全默认 1000 */
    int16_t YAW;           /* 偏航通道，默认回中 1500 */
    int16_t PIT;           /* 俯仰通道，默认回中 1500 */
    int16_t ROL;           /* 横滚通道，默认回中 1500 */

    int16_t OffSet_Pit;    /* 俯仰微调 */
    int16_t OffSet_Rol;    /* 横滚微调 */
    int16_t OffSet_Yaw;    /* 偏航微调 */

    uint16_t RxCount;      /* 收到有效遥控包的计数，用于初步通信测试 */
    uint16_t LostCount;    /* 连续未收到有效遥控包的周期计数 */
} _st_RemoterControl;


typedef struct
{
    uint8_t MPU6050_Ready; /* MPU6050 是否已成功读取到有效数据 */
    int16_t ACC_X;         /* 加速度 X 轴，使用 MPU6050 滤波后的原始量 */
    int16_t ACC_Y;         /* 加速度 Y 轴 */
    int16_t ACC_Z;         /* 加速度 Z 轴 */
    int16_t GYRO_X;        /* 角速度 X 轴，使用 MPU6050 滤波后的原始量 */
    int16_t GYRO_Y;        /* 角速度 Y 轴 */
    int16_t GYRO_Z;        /* 角速度 Z 轴 */
    int16_t MAG_X;         /* 当前无磁力计，回传时暂填 0 */
    int16_t MAG_Y;         /* 当前无磁力计，回传时暂填 0 */
    int16_t MAG_Z;         /* 当前无磁力计，回传时暂填 0 */
    uint16_t BatteryMv;    /* 飞控电压，单位 mV；未接入 ADC 时为 0 */
} _st_FlySensor;

typedef struct
{
    uint8_t unlock;         /* 1 表示允许电机输出，0 表示锁定并强制电机停转 */
} _st_FlyContrlFlag;

extern _st_Angle FlyAngle;
extern _st_RemoterControl FlyRemoter;
extern _st_FlySensor FlySensor;
extern _st_FlyContrlFlag FlyContrlFlag;

#endif
