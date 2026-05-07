#include "stm32f10x.h"                  // Device header
#include "led.h"

//硬件：LED正连接在PA0上，LED负连接在GND




//初始化方法
void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	//默认状态 ---关闭灯
	
	LED1_Off();
	LED2_Off();
}

//开灯
void LED1_On(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_0);
}

//关灯
void LED1_Off(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_0);
}

//开灯
void LED2_On(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_1);
}

//关灯
void LED2_Off(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_1);
}

void LED2_Toggle(void)
{
	GPIO_Write(GPIOA,GPIOA->ODR^GPIO_Pin_1); //异或
}
