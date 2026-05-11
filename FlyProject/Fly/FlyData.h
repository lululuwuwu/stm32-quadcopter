/*
 * 文件名: FlyData.h
 * 功能说明:
 * 1. _st_Angle        飞机姿态欧拉角结构体，保存 Pitch/Roll/Yaw。
 * 2. FlyAngle         当前飞机姿态角全局变量声明，由 MPU6050 姿态任务更新。
 *
 * 本文件用于集中声明飞控业务层共享数据。后续如果加入遥控通道、电池电压、传感器状态，
 * 建议继续放在 FlyData 模块统一维护，避免多个任务随意散落全局变量。
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

extern _st_Angle FlyAngle;

#endif
