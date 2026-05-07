#include "board.h"


void vTaskSerial1SendLogCode(void * pvParameters)
{
	Serial1_Init();//初始化
	
	char pvBuffer[uxQueueSerial1ItemSize];//日志日志接收缓冲区

	while(1)
	{
		//等待队列
		xQueueReceive(xQueueSerial1,pvBuffer,portMAX_DELAY); //无限等待
		Serial1_SendString(pvBuffer);
	}
}


void vTaskOLEDShow(void *paramters)
{
	Show_Init();
	// 修改数据测试一下
	RemoterData.windows = 3;
	RemoterData.NRF_Channel = 28;
	RemoterData.NRF_Connect = 1;
	RemoterData.RC_low_power = 0;
	RemoterData.Fly_low_power = 1;
	RemoterData.NRF_RSSI_count = 51;
	RemoterData.Battery_RC = 3789;
	RemoterData.Battery_Fly = 3678;
	RemoterData.THR = 1111;
	RemoterData.ROL = 1112;
	RemoterData.YAW = 1113;
	RemoterData.PIT = 1114;
	RemoterData.OffSet_Pit = 12;
	RemoterData.OffSet_Rol = 13;
	RemoterData.OffSet_Yaw = 14;
	RemoterData.X = 101;
	RemoterData.Y = 102;
	RemoterData.Z = 103;
	RemoterData.H = 104;

	RemoterData.fly_test_flag = 0x03;
	
	while(1)
	{
		Show_Refresh();
		vTaskDelay(500);
	}
}





// 有很多任务需要创建，那么我们可以创建一个任务来创建任务
void RemoterCreateTask(void *paramters)
{
	// 希望创建任务的时候，其他任务和中断无法打断执行

	// 进入临界
	taskENTER_CRITICAL();


	xTaskCreate(vTaskSerial1SendLogCode,"TaskSerial1Log",256,NULL,(configMAX_PRIORITIES -1),NULL);
	
	xTaskCreate(vTaskOLEDShow,"TaskOLEDShow",256,NULL,1,NULL);


	// 退出临界
	taskEXIT_CRITICAL();

	vTaskDelete(NULL);
}
