#include "stm32f10x.h"                  // Device header

//硬件：A相连接在PA6上，B相连接在PA7

int32_t MyEncoder_CountValue=0x0;

//初始化
void MyEncoder_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//模式：浮空
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource7);
	
	EXTI_InitTypeDef EXTI_InitTypeStructure;
	EXTI_InitTypeStructure.EXTI_Line = EXTI_Line7;
	EXTI_InitTypeStructure.EXTI_LineCmd = ENABLE;
	EXTI_InitTypeStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitTypeStructure.EXTI_Trigger = EXTI_Trigger_Rising; //上升沿触发
	
	EXTI_Init(&EXTI_InitTypeStructure);
	
	
	NVIC_InitTypeDef NVIC_InitTypeStructure;
	NVIC_InitTypeStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_InitTypeStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority = 0;
	
	NVIC_Init(&NVIC_InitTypeStructure);
	
}

//获取编码器的值

int32_t MyEncoder_GetValue(void)
{
	return MyEncoder_CountValue;
}


void EXTI9_5_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line7)==SET)
	{
		if(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_6)==SET)
		{
			MyEncoder_CountValue--;
		}else
		{
			MyEncoder_CountValue++;
		}
		
		
		EXTI_ClearITPendingBit(EXTI_Line7);
	}
}




