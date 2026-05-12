/*
 * 文件名: FlyAttitude.h
 * 功能说明:
 * 1. FlyAttitudeTask_20ms  MPU6050 姿态任务入口，负责传感器就绪、姿态融合和串口打印。
 *
 * 本模块集中管理 MPU6050 姿态业务，避免把四元数融合、角度打印等细节塞进任务创建模块。
 */
#ifndef __FLYATTITUDE_H
#define __FLYATTITUDE_H

void FlyAttitudeTask_20ms(void *pvParameters);

#endif
