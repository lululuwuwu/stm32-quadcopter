#include "stm32f10x.h"                  // Device header


//初始化编码器
void MyTIM2Encoder_Init(void)
{
	//1.开启时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //模式： 浮空输入
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	//2.设置时钟来源为内部时钟
//	TIM_InternalClockConfig(TIM2); //TIM2默认使用的就是内部时钟， 这句话可以不写
	
	//3.初始化
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitTypeStructure;
	TIM_TimeBaseInitTypeStructure.TIM_ClockDivision = TIM_CKD_DIV1; //时钟分频 --->没有用
	TIM_TimeBaseInitTypeStructure.TIM_CounterMode =TIM_CounterMode_Up; //向上计数
	TIM_TimeBaseInitTypeStructure.TIM_Period = 65536-1;//(ARR)Auto-Reload Register,   between 0x0000 and 0xFFFF. 
	TIM_TimeBaseInitTypeStructure.TIM_Prescaler = 4-1; //(PSC) prescaler value,between 0x0000 and 0xFFFF 
	TIM_TimeBaseInitTypeStructure.TIM_RepetitionCounter = 0;//重复计数器  only for TIM1 and TIM8. between 0x00 and 0xFF.
	
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitTypeStructure);
	
	//4.配置编码器接口
	//TIM_EncoderMode_TI12 在A和B相上都计数
	
	//A相（IC1） 和 B相（IC2） 都不要电平反转
	TIM_EncoderInterfaceConfig(TIM2,TIM_EncoderMode_TI12,TIM_ICPolarity_Rising,TIM_ICPolarity_Rising);
	
	
	//5.设置CNT的默认值
	TIM_SetCounter(TIM2,0);
	
	//6.启动
	
	TIM_Cmd(TIM2,ENABLE);
	
	
	
}


//获取编码器读数
int16_t MyTIM2Encoder_GetValue(void)
{

	return TIM_GetCounter(TIM2);
}



