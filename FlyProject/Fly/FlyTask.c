/*
 * 文件名: FlyTask.c
 * 功能说明:
 * 1. xQueueSerial3            串口 3 日志队列句柄定义。
 * 2. TaskSerial3              串口日志发送任务，从队列取字符串并发送到 USART3。
 * 3. LEDTask                  LED 状态灯任务，周期执行 LED_Blink。
 * 4. FlyContrlMotorTask_3ms   电机输出任务，每 3ms 刷新一次四路 PWM。
 * 5. FlyNRF24L01Task_20ms     NRF24L01 无线接收任务，接收遥控器数据并回传 ACK 状态。
 * 6. FlyCreateTask            飞控任务统一创建入口，集中管理任务栈、优先级和创建顺序。
 *
 * 调度说明:
 * - 本文件只负责 FreeRTOS 任务组织，不再承载具体姿态算法和传感器业务。
 * - MPU6050 姿态读取和四元数融合放在 FlyAttitude 模块中。
 */
#include "board.h"
#include "FlyAttitude.h"

#define FLY_UNLOCK_PERIOD_MS 10
#define FLY_NRF24L01_PERIOD_MS 20

// 串口 3 日志队列句柄。main.c 创建队列，本文件定义句柄存储空间。
QueueHandle_t xQueueSerial3;

void TaskSerial3(void *pvParameters)
{
    char msg[uxQueueSerial3ItemSize];

    Serial3_Init();

    while (1)
    {
        // 没有日志时永久阻塞，有日志时取出一条并串口发送。
        // 串口实际发送是阻塞的，因此它独立成任务，避免业务任务被发送耗时影响。
        xQueueReceive(xQueueSerial3, msg, portMAX_DELAY);
        Serial3_SendString(msg);
    }
}

void LEDTask(void *pvParameters)
{
    LED_Init();

    // 当前使用流水灯作为运行状态提示，后续可由飞控状态机切换 AlwaysOn/AlwaysOff 等模式。
    setLED.status = OneByOne;

    Serial3_SendLog("LED_Init ... \n");
    while (1)
    {
        LED_Blink();
    }
}

void FlyContrlMotorTask_3ms(void *pvParameters)
{
    FlyContrl_Init();

    // 手动测试电机时再打开，避免上电后电机误转。
    // MOTOR1 = 60;
    // MOTOR2 = 60;
    // MOTOR3 = 60;
    // MOTOR4 = 60;

    while (1)
    {
        vTaskDelay(3);
        FlyContrl_Motor_3ms();
    }
}

void FlyUnlockTask_10ms(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        FlyContrl_Unlock_10ms();
        vTaskDelay(pdMS_TO_TICKS(FLY_UNLOCK_PERIOD_MS));
    }
}

void FlyNRF24L01Task_20ms(void *pvParameters)
{
    (void)pvParameters;

    NRF24L01_Init(NRF_MODEL_RX, NRF24L01_DEFAULT_CHANNEL);
    Serial3_SendLog("NRF24L01 RX init, channel:%d\r\n", NRF24L01_DEFAULT_CHANNEL);

    while (1)
    {
        NRF24L01_FlyUpdate();
        vTaskDelay(pdMS_TO_TICKS(FLY_NRF24L01_PERIOD_MS));
    }
}

void FlyCreateTask(void *pvParameters)
{
    // 创建多个任务时进入临界区，避免任务创建到一半就被调度打断。
    taskENTER_CRITICAL();

    xTaskCreate(TaskSerial3, "TaskSerial3", 128, NULL, (configMAX_PRIORITIES - 1), NULL);
    xTaskCreate(LEDTask, "LEDTask", 128, NULL, 1, NULL);
    xTaskCreate(FlyContrlMotorTask_3ms, "FlyContrlMotorTask_3ms", 128, NULL, 2, NULL);
    xTaskCreate(FlyUnlockTask_10ms, "FlyUnlockTask_10ms", 128, NULL, 2, NULL);
    xTaskCreate(FlyAttitudeTask_20ms, "MPU6050Task", 256, NULL, 1, NULL);
    xTaskCreate(FlyNRF24L01Task_20ms, "NRF24L01Task", 256, NULL, 2, NULL);

    taskEXIT_CRITICAL();

    // 创建器任务只负责启动其他任务，完成后删除自身释放栈空间。
    vTaskDelete(NULL);
}
