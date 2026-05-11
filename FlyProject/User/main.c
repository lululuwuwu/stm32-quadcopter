/*
 * 文件名: main.c
 * 功能说明:
 * 1. main              飞机端程序入口。
 *
 * 本文件负责完成系统启动阶段的最小初始化：
 * - 配置 NVIC 中断优先级分组，保证 FreeRTOS 和外设中断使用同一套优先级规则。
 * - 创建串口 3 日志队列 xQueueSerial3，供各任务通过 Serial3_SendLog() 异步打印日志。
 * - 创建 FlyCreateTask，由该任务统一创建串口、LED、电机刷新、MPU6050 姿态读取等业务任务。
 * - 启动 FreeRTOS 调度器，之后 CPU 由各任务按优先级和延时节拍调度。
 */
#include "board.h"

int main(void)
{
    // 一个 STM32 工程只能设置一套 NVIC 优先级分组。
    // FreeRTOS 中断安全 API 依赖该分组方式，工程内不要在其他地方再次修改。
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    // 创建串口日志队列。
    // 队列项大小由 uxQueueSerial3ItemSize 决定，Serial3_SendLog 会把格式化后的字符串送入该队列。
    xQueueSerial3 = xQueueCreate(uxQueueSerial3Length, uxQueueSerial3ItemSize);

    // 创建任务创建器。真正的业务任务在 FlyCreateTask 中统一创建，便于集中管理栈大小和优先级。
    xTaskCreate(FlyCreateTask, "FlyCreateTask", 128, NULL, 1, NULL);

    // 启动 FreeRTOS 调度器。正常情况下该函数不会返回。
    vTaskStartScheduler();

    // 如果运行到这里，通常说明 FreeRTOS 堆空间不足或调度器启动失败。
    while (1)
    {
    }
}
