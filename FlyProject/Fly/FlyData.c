/*
 * 文件名: FlyData.c
 * 功能说明:
 * 1. FlyAngle    当前飞机姿态角全局数据，单位为度。
 * 2. FlyRemoter  飞控端接收到的遥控器通道和链路状态。
 *
 * 本文件只负责存放飞控共享状态，不直接访问外设。
 * 目前 FlyAngle 由 FlyAttitudeTask_20ms 周期更新，FlyRemoter 由 NRF24L01 任务更新。
 */
#include "board.h"
#include "FlyData.h"

// 上电默认姿态清零；实际角度会在 MPU6050 姿态读取任务启动后逐步刷新。
_st_Angle FlyAngle = {0.0f, 0.0f, 0.0f};

// 遥控通道使用安全默认值：油门最低，其他通道回中，避免无线未连接时误给油。
_st_RemoterControl FlyRemoter = {
    0,
    0,
    NRF24L01_DEFAULT_CHANNEL,
    0,
    1000,
    1500,
    1500,
    1500,
    0,
    0,
    0,
    0,
    0
};

// 飞控回传用的传感器快照。电压 ADC 尚未接入时 BatteryMv 保持 0。
_st_FlySensor FlySensor = {0};

// 上电默认保持锁定，必须完成油门低-高-低流程后才允许电机输出。
_st_FlyContrlFlag FlyContrlFlag = {0};
