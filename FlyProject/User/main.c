#include "board.h"


int main(void)
{	
	
	//1.先设置优先级分组 --- 一个工程Project统一成一个分组。
	/*
	
 The table below gives the allowed values of the pre-emption priority and subpriority according
 to the Priority Grouping configuration performed by NVIC_PriorityGroupConfig function
  ============================================================================================================================
    NVIC_PriorityGroup   | NVIC_IRQChannelPreemptionPriority | NVIC_IRQChannelSubPriority  | Description
  ============================================================================================================================
   NVIC_PriorityGroup_0  |                0                  |            0-15             |   0 bits for pre-emption priority
                         |                                   |                             |   4 bits for subpriority
  ----------------------------------------------------------------------------------------------------------------------------
   NVIC_PriorityGroup_1  |                0-1                |            0-7              |   1 bits for pre-emption priority
                         |                                   |                             |   3 bits for subpriority
  ----------------------------------------------------------------------------------------------------------------------------    
   NVIC_PriorityGroup_2  |                0-3                |            0-3              |   2 bits for pre-emption priority
                         |                                   |                             |   2 bits for subpriority
  ----------------------------------------------------------------------------------------------------------------------------    
   NVIC_PriorityGroup_3  |                0-7                |            0-1              |   3 bits for pre-emption priority
                         |                                   |                             |   1 bits for subpriority
  ----------------------------------------------------------------------------------------------------------------------------    
   NVIC_PriorityGroup_4  |                0-15               |            0                |   4 bits for pre-emption priority
                         |                                   |                             |   0 bits for subpriority                       
  ============================================================================================================================
	
	*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);  //一个工程只有一个优先级组分配
		
	//创建队列
    xQueueSerial3 = xQueueCreate(uxQueueSerial3Length, uxQueueSerial3ItemSize);	
		
	//创建任务	
	xTaskCreate(FlyCreateTask, "FlyCreateTask", 128, NULL, 1, NULL);
	
	//启动任务调度
	vTaskStartScheduler();
	

}
