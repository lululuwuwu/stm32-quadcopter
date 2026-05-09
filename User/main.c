#include "board.h"
#include "task.h"
#include "remotetask.h"

int main(void)
{
    // 1. 先设置优先级分组 --- 一个工程 Project 统一成一个分组。
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    // 创建队列。
    xQueueSerial1 = xQueueCreate(uxQueueSerial1Length, uxQueueSerial1ItemSize);

    // 创建任务。
    xTaskCreate(RemoterCreateTask, "RemoterCreateTask", 128, NULL, 1, NULL);

    // 启动任务调度。
    vTaskStartScheduler();
}
