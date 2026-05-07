#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"

//硬件： KEY的一头连接在PB0， 一头连接在GND
//分析： GPIO模式 ---> 上拉输入

//初始化硬件
void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU; //上拉输入
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitTypeStructure);

}

//检测按键是否被按下（1就是按了一次，0就是没有按）
uint8_t Key1_Pressed(void)
{
	if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_0)==0)
	{
		//软件消除抖动
		vTaskDelay(10); //任务延时
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_0)==0); //消耗低电平时间
		vTaskDelay(10);
		//按了
		return 1;
	}
	
	return 0;
}

uint8_t Key2_Pressed(void)
{
	if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)==0)
	{
		//软件消除抖动
		vTaskDelay(10);
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)==0); //消耗低电平时间
		vTaskDelay(10);
		//按了
		return 1;
	}
	
	return 0;
}

