#include "board.h"

// 创建队列句柄
QueueHandle_t xQueueSerial3;

// 串口任务
void TaskSerial3(void *pvParameters)
{
	char msg[uxQueueSerial3ItemSize];

	Serial3_Init();

	while (1)
	{
		// 如果队列里面有数据就获取一个，发送串口；如果没有数据就阻塞无限等待

		/*

		如果从队列中成功接收项目，则返回 pdTRUE； 否则返回 pdFALSE。
		BaseType_t xQueueReceive(
						  QueueHandle_t xQueue, //要从中接收项目的队列的句柄。
						  void *pvBuffer,  //指向要将所接收项目复制到缓冲区的指针。
						  TickType_t xTicksToWait //如果队列为空，将 xTicksToWait 设置为 0 将导致函数立即返回 ;portMAX_DELAY 会导致任务无限期地阻塞 （没有超时）
		);

		*/

		xQueueReceive(xQueueSerial3, msg, portMAX_DELAY);
		Serial3_SendString(msg);
	}
}

// 灯控
void LEDTask(void *pvParameters)
{
	LED_Init();
	// 测试状态
	setLED.status = OneByOne; // OneByOne,AllFlashLight,AlternateFlash
	
	Serial3_SendLog("LED_Init ... \n");
	while (1)
	{
		LED_Blink();
	}
}


// 飞行控制
void FlyContrlMotorTask_3ms(void *pvParameters)
{
	FlyContrl_Init();
	// 测试一下
//	MOTOR1 = 60;
//	MOTOR2 = 60;
//	MOTOR3 = 60;
//	MOTOR4 = 60;

	while (1)
	{
		vTaskDelay(3);

		FlyContrl_Motor_3ms();
	}
}


// 有很多任务需要创建，那么我们可以创建一个任务来创建任务
void FlyCreateTask(void *pvParameters)
{
	// 进入临界 ,希望创建任务的时候，其他任务和中断无法打断执行
	taskENTER_CRITICAL();

	xTaskCreate(TaskSerial3, "TaskSerial3", 128, NULL, (configMAX_PRIORITIES -1), NULL);
	xTaskCreate(LEDTask, "LEDTask", 128, NULL, 1, NULL);
	xTaskCreate(FlyContrlMotorTask_3ms, "FlyContrlMotorTask_3ms", 128, NULL, 2, NULL);


	// 退出临界
	taskEXIT_CRITICAL();

	vTaskDelete(NULL);
}
