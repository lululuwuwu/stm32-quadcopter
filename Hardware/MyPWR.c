#include "stm32f10x.h"                  // Device header
#include "led.h"

void MyPWR_Init(void)
{

	/*
	用户可以利用PVD对VDD电压与电源控制寄存器(PWR_CR)中的【PLS[2:0]位】进行比较来监控电源，这几位选择监控电压的阀值。 
    通过设置【PVDE位来使能PVD】。 
   电源控制/状态寄存器(PWR_CSR)中的PVDO标志用来表明VDD是高于还是低于PVD的电压阀值。
	该事件在内部连接到【外部中断的第16线】，如果该中断在外部中断寄存器中是使能的，该事件就会产生中断。
	当VDD下降到PVD阀值以下和(或)当VDD上升到PVD阀值之上时，根据外部中断第16线的上升/下降边沿触发设置，
	就会产生PVD中断。例如，这一特性可用于用于执行紧急关闭任务。 	
	
	*/
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR,ENABLE);
	
	//配置PLS[2:0]
	PWR_PVDLevelConfig(PWR_PVDLevel_2V5);
	
	//使能
	PWR_PVDCmd(ENABLE);
	
	//配置外部中断引脚线16
	EXTI_InitTypeDef EXTI_InitTypeStructure;
	EXTI_InitTypeStructure.EXTI_Line = EXTI_Line16; //外部中断的第16线 PVD
	EXTI_InitTypeStructure.EXTI_LineCmd = ENABLE;
	EXTI_InitTypeStructure.EXTI_Mode = EXTI_Mode_Interrupt; //中断模式，通往CPU处理
	EXTI_InitTypeStructure.EXTI_Trigger = EXTI_Trigger_Rising; //触发 ，电压下降是上升沿
	
	EXTI_Init(&EXTI_InitTypeStructure);
	
	//NVIC
	NVIC_InitTypeDef NVIC_InitTypeStructure;
	NVIC_InitTypeStructure.NVIC_IRQChannel = PVD_IRQn;
	NVIC_InitTypeStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority = 0;
	
	NVIC_Init(&NVIC_InitTypeStructure);
	
	
}


void PVD_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line16)==SET)
	{
			LED1_Off();
		
		EXTI_ClearITPendingBit(EXTI_Line16);
	}

	
}

