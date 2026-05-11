/*
 * 文件名: FlyData.c
 * 功能说明:
 * 1. FlyAngle         当前飞机姿态角全局数据，单位为度。
 *
 * 本文件只负责存放飞控共享状态，不直接访问外设。
 * 目前 FlyAngle 由 MPU6050AttitudeTask_20ms 周期更新，串口任务只读取并打印该数据。
 */
#include "board.h"
#include "FlyData.h"

// 上电默认姿态清零；实际角度会在 MPU6050 姿态读取任务启动后逐步刷新。
_st_Angle FlyAngle = {0.0f, 0.0f, 0.0f};
