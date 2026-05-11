/*
 * 文件名: FlyTask.h
 * 功能说明:
 * 1. FlyCreateTask  飞控任务统一创建入口。
 *
 * FlyCreateTask 由 main.c 创建，随后在临界区中创建串口、LED、电机刷新和 MPU6050 姿态任务。
 */
#ifndef __FLYTASK_H
#define __FLYTASK_H

void FlyCreateTask(void *paramters);

#endif
