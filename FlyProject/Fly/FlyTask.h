/*
 * 文件名: FlyTask.h
 * 功能说明:
 * 1. FlyCreateTask  飞控任务统一创建入口。
 *
 * FlyCreateTask 由 main.c 创建，随后在临界区中创建串口、LED、电机刷新、姿态读取和 NRF 通信任务。
 */
#ifndef __FLYTASK_H
#define __FLYTASK_H

void FlyCreateTask(void *parameters);

#endif
