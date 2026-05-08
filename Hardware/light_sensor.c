#include "stm32f10x.h"                  // Device header

//硬件： 光敏传感器连接在PA2上

//初始化
void LightSensor_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU; //模式： 上拉(√)，下拉（x），浮空（√） ，模拟 （x）
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	
}

//获取光线强度: 1- 光线弱  ； 0 - 光线强
uint8_t LightSensor_GetValue(void)
{
	return GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_2);
}
